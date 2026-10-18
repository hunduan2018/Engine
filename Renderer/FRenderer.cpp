// FRenderer.cpp

#include "FRenderer.h"

#include "../src/Assert.h"
#include "D3D12QueueManger.h"
#include "FBasePassProcessor.h"
#include "FMeshPassProcessor.h"
#include "FStaticMeshVertexFactory.h"
#include "d3dUtil.h"

#include <cstring>

void FRenderer::Initialize(ID3D12Device* device,
                           ID3D12CommandAllocator* initAllocator,
                           ID3D12GraphicsCommandList* commandList,
                           Direct3DQueue* graphicsQueue,
                           DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
{
    Device = device;
    InitCommandList = commandList;
    GraphicsQueue = graphicsQueue;
    RtvFormat = rtvFormat;
    DsvFormat = dsvFormat;

    GlobalRootSignature.Initialize(device);
    PsoCache.Initialize(device, &ShaderManager, GlobalRootSignature.Get(), rtvFormat, dsvFormat);
    // 默认静态网格工厂最先注册（句柄 0），材质/网格可直接取用。
    StaticMeshVertexFactory = RegisterVertexFactory(std::make_unique<FStaticMeshVertexFactory>());
    Bindless = std::make_unique<BindlessAllocator>(device, RendererLimits::BindlessSrvCount);
    ConstantRing.Initialize(device, RendererLimits::NumFrameSlots, 1024 * 1024);
    InstanceAllocator.Initialize(device);

    // bindless 堆槽 0 固定给实例缓冲（着色器 t0），必须最先分配。
    {
        D3D12_CPU_DESCRIPTOR_HANDLE instanceSrvHandle = {};
        const uint32_t slot = Bindless->Allocate(instanceSrvHandle);
        APP_CHECK_MSG(slot == 0, "Instance buffer SRV must own bindless heap slot 0");
        InstanceAllocator.CreateInstanceBufferSRV(device, instanceSrvHandle);
    }

    // 命令列表进入录制状态：随后的网格/纹理创建在其上录制上传命令。
    D3D12_THROW_IF_FAILED(InitCommandList->Reset(initAllocator, nullptr));
}

void FRenderer::FlushInitUploads()
{
    UploadContext.FlushAndWait(GraphicsQueue, InitCommandList);
}

const FVertexFactory* FRenderer::RegisterVertexFactory(std::unique_ptr<FVertexFactory> factory)
{
    APP_CHECK(factory != nullptr);
    factory->Handle = static_cast<FVertexFactoryHandle>(VertexFactories.size());
    const FVertexFactory* result = factory.get();
    VertexFactories.push_back(std::move(factory));
    return result;
}

void FRenderer::RegisterMaterial(FMaterial* material, const FVertexFactory* vertexFactory)
{
    APP_CHECK(material != nullptr);
    APP_CHECK_MSG(vertexFactory != nullptr && vertexFactory->Handle != InvalidVertexFactoryHandle,
        "Material must be registered with a registered vertex factory");
    APP_CHECK_MSG(material->VertexShader != InvalidShaderHandle
        && material->PixelShader != InvalidShaderHandle,
        "Material must have valid vertex/pixel shaders before registration");

    // v1：所有通道用同一状态组合（不透明、深度按通道决定）。
    // 后续通道需要不同状态时，在此按 Pass 组装各自的 desc。
    // 布局来自工厂；拓扑由材质管线决定（曲面细分只影响 TopologyType，不影响布局）。
    FGraphicsPipelineStateDesc desc(
        material->VertexShader, material->PixelShader, vertexFactory->Handle,
        material->HullShader, material->DomainShader,
        EBlendMode::Opaque, material->Rasterizer, EDepthMode::ReadWrite,
        material->IsTessellated()
            ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH
            : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    // 输出布局由渲染器填充（交换链 RTV/DSV 格式）。
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = RtvFormat;
    desc.DSVFormat = DsvFormat;
    desc.SampleCount = 1;

    material->PipelineStates[static_cast<size_t>(EMeshPass::BasePass)] = PsoCache.GetOrCreate(desc, vertexFactory);
    material->SortId = NextMaterialSortId++;
}

void FRenderer::BeginFrame(uint32_t frameSlot)
{
    CurrentFrameSlot = frameSlot;
    ConstantRing.BeginFrame(frameSlot);
    InstanceAllocator.BeginFrame(frameSlot);
    MaterialConstants.Reset();
    for (FMeshPassDrawList& drawList : PassDrawLists)
    {
        drawList.Reset();
    }
    DrawSubmitter.ResetCache();
}

void FRenderer::UpdateView(const FSceneView& view)
{
    CurrentView = view;

    // 转置上传（着色器 mul(向量, 矩阵) 约定，与本项目 cbuffer 上传约定一致）。
    FViewConstants viewConstants = {};
    {
        using namespace DirectX;
        FSimdMatrix matrix = XMMatrixTranspose(view.View);
        XMStoreFloat4x4(&viewConstants.View, matrix);
        matrix = XMMatrixTranspose(view.ViewProjection);
        XMStoreFloat4x4(&viewConstants.ViewProjection, matrix);
    }
    viewConstants.ViewOriginTime = FVector4(
        view.ViewOrigin.x, view.ViewOrigin.y, view.ViewOrigin.z, view.TimeSeconds);
    viewConstants.LightDirection = FVector4(
        view.LightDirection.x, view.LightDirection.y, view.LightDirection.z, 0.0f);
    viewConstants.Resolution = FVector4(view.ViewportWidth, view.ViewportHeight, 0.0f, 0.0f);

    const FRingAllocation allocation = ConstantRing.Alloc(
        CurrentFrameSlot, sizeof(FViewConstants));
    std::memcpy(allocation.CpuAddress, &viewConstants, sizeof(viewConstants));
    ViewCbGpuAddress = allocation.GpuAddress;
}

void FRenderer::RenderScene(const FScene& scene, ID3D12GraphicsCommandList* commandList)
{
    // 批次构建：同 (Mesh, Section, Material) 实例合并。
    scene.BuildMeshBatches(TmpBatches);

    // BasePass 处理器：批次 -> 命令。
    FMeshPassProcessorContext context = {};
    context.ConstantRing = &ConstantRing;
    context.MaterialConstants = &MaterialConstants;
    context.Instances = &InstanceAllocator;
    context.RootSignature = GlobalRootSignature.Get();
    context.ViewCbGpuAddress = ViewCbGpuAddress;
    context.FrameSlot = CurrentFrameSlot;
    context.View = CurrentView;

    FMeshPassDrawList& basePassList = PassDrawLists[static_cast<size_t>(EMeshPass::BasePass)];
    FBasePassProcessor baseProcessor(context, basePassList);
    for (const FMeshBatch& batch : TmpBatches)
    {
        baseProcessor.AddMeshBatch(batch);
    }

    // 本帧实例矩阵：暂存 -> 默认堆（必须在读取它的绘制提交之前）。
    InstanceAllocator.FlushInstanceCopies(commandList);

    // 排序后逐条提交。
    basePassList.Sort();
    DrawSubmitter.ExecuteDraws(basePassList, commandList, PsoCache, Bindless->GetHeap());
}
