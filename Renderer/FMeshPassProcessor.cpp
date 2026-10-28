// FMeshPassProcessor.cpp

#include "FMeshPassProcessor.h"

#include "../src/Assert.h"
#include "FGlobalRootSignature.h"
#include "FInstanceAllocator.h"

FMeshPassProcessor::FMeshPassProcessor(
    FMeshPassProcessorContext& inContext, FMeshPassDrawList& inDrawList, EMeshPass inPassId)
    : Context(inContext)
    , DrawList(inDrawList)
    , PassId(inPassId)
{
}

void FMeshPassProcessor::BuildMeshDrawCommand(const FMeshBatch& batch)
{
    APP_CHECK(batch.Mesh != nullptr && batch.Material != nullptr);
    APP_CHECK(batch.SectionIndex < batch.Mesh->Sections.size());

    const FMaterial& material = *batch.Material;
    // View Mode（如全局 Wireframe）在此生效：材质不感知视图状态，只提供各模式的预建 PSO。
    const FPSOHandle psoHandle =
        material.PipelineStates[static_cast<size_t>(PassId)][static_cast<size_t>(Context.View.FillMode)];
    APP_CHECK_MSG(psoHandle != InvalidPSOHandle,
        "Material was not registered for this pass; PSOs are created at registration time only");

    // 材质常量走每帧缓存：同材质多批次共享一次环分配与同一 CBV 地址。
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbGpuAddress = Context.MaterialConstants->GetOrCreate(
        &material, *Context.ConstantRing, Context.FrameSlot);

    // 实例矩阵写入实例缓冲，返回绝对起始下标。
    const uint32_t startInstanceLocation = Context.Instances->Alloc(
        Context.FrameSlot, batch.Instances);

    FMeshDrawCommand command = {};

    // ---- 管线 ----
    command.PipelineState = psoHandle;
    command.RootSignature = Context.RootSignature;

    // ---- 根绑定：b0 视图 / b1 材质 / b2 实例偏移根常量 ----
    {
        FMeshDrawCommand::FRootBinding& viewBinding = command.RootBindings[command.RootBindingCount++];
        viewBinding.Type = ERootBindingType::CBV;
        viewBinding.SlotIndex = RootParam::ViewCBV;
        viewBinding.GpuAddress = Context.ViewCbGpuAddress;

        FMeshDrawCommand::FRootBinding& materialBinding = command.RootBindings[command.RootBindingCount++];
        materialBinding.Type = ERootBindingType::CBV;
        materialBinding.SlotIndex = RootParam::MaterialCBV;
        materialBinding.GpuAddress = materialCbGpuAddress;

        FMeshDrawCommand::FRootBinding& constantsBinding = command.RootBindings[command.RootBindingCount++];
        constantsBinding.Type = ERootBindingType::Constants;
        constantsBinding.SlotIndex = RootParam::DrawConstants;
        constantsBinding.NumConstants = 4;
        constantsBinding.Constants[0] = startInstanceLocation;
        // Constants[1..3] 预留：第二阶段 MeshShading 的 VB/IB/meshlet SRV 索引。
    }

    // ---- 几何流：流数由网格的顶点工厂描述（v1 单流交错）+ 索引缓冲 ----
    const FMeshRenderDataSectionRange& section = batch.Mesh->Sections[batch.SectionIndex];
    command.VertexBufferViews[0] = batch.Mesh->VertexBufferView;
    command.VertexStreamCount = batch.Mesh->Factory->GetStreamCount();
    command.IndexBufferView = batch.Mesh->IndexBufferView;
    command.PrimitiveTopology = material.IsTessellated()
        ? D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST
        : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    command.DrawType = EDrawType::Indexed;

    // ---- 绘制参数 ----
    command.IndexCountPerInstance = section.IndexCount;
    command.InstanceCount = static_cast<uint32_t>(batch.Instances.size());
    command.StartIndexLocation = section.IndexStart;
    command.BaseVertexLocation = section.VertexBase;
    command.StartInstanceLocation = startInstanceLocation;

    // ---- 排序键：PSO -> 材质 -> 序列（序列号即命令在桶中的下标）----
    const uint16_t sequence = static_cast<uint16_t>(DrawList.Num());
    command.SortKey = ComposeOpaqueSortKey(
        static_cast<uint16_t>(PassId),
        static_cast<uint16_t>(psoHandle),
        material.SortId,
        sequence);
    DrawList.Add(command);
}
