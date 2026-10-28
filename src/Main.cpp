//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "Assert.h"
#include "D3DApp.h"
#include "MathHelper.h"

#include "FRenderer.h"
#include "FMeshRenderData.h"
#include "FTextureResource.h"
#include "Mesh/FGridMeshBuilder.h"

#include "occcity.h"

#include <cstdio>

class TestApp : public D3DApp
{
public:
    TestApp(HINSTANCE hInstance)
        : D3DApp(hInstance, L"TestApp", 1280, 720)
    {
    }
    ~TestApp() override = default;

    virtual bool Initialize() override
    {
        if (!D3DApp::Initialize())
        {
            return false;
        }

        if (!InitializeRenderer())
        {
            return false;
        }

        return true;
    }

 private:
     virtual void OnResize() override
     {
         D3DApp::OnResize();
     }

     virtual void Update(const GameTimer& timer) override
     {
         const float deltaTime = timer.DeltaTime();
         ElapsedTime += deltaTime;

         // 周期性输出渲染命令统计（M2/M3 验证：命令数、实例合并、PSO 状态跳过）。
         if (++FrameCounter % 120 == 0)
         {
             char buffer[128] = {};
             std::snprintf(buffer, sizeof(buffer),
                 "[MEngine] BasePass commands=%u psoSetCount=%u\n",
                 Renderer->GetPassCommandCount(EMeshPass::BasePass),
                 Renderer->GetLastPipelineStateSetCount());
             OutputDebugStringA(buffer);
         }
     }

     virtual void Draw(const GameTimer& timer) override
     {
         const UINT frameIndex = mCurrentBackBuffer;
         const UINT64 frameFence = mFrameFenceValues[frameIndex];
         if (frameFence != 0)
         {
             mQueueManager->GetGraphicsQueue()->WaitForFenceCPUBlocking(frameFence);
         }

         // 渲染器帧状态重置必须发生在该帧槽 fence 等待之后（上传环/实例缓冲生命周期约定）。
         Renderer->BeginFrame(frameIndex);

         // Reset the command list allocator for the current frame
         D3D12_THROW_IF_FAILED(mDirectCmdListAlloc[frameIndex]->Reset());

         // Reset the command list with the current command allocator and no initial pipeline state
         D3D12_THROW_IF_FAILED(mCommandList->Reset(mDirectCmdListAlloc[frameIndex].Get(), nullptr));

         mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
             CurrentBackBuffer(),
             D3D12_RESOURCE_STATE_PRESENT,
             D3D12_RESOURCE_STATE_RENDER_TARGET));

         mCommandList->RSSetScissorRects(1, &mScissorRect);
         mCommandList->RSSetViewports(1, &mScreenViewport);

         const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
             mRtvHeap->GetCpuHandle(mCurrentBackBuffer);
         const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
             mDsvHeap->GetCpuHandle(0);

         mCommandList->ClearRenderTargetView(rtvHandle, Colors::Black, 0, nullptr);
         mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

         mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

         // ---- Mesh Draw Command 流程：视图常量 -> 场景渲染 ----
         FSceneView view = {};
         view.View = Camera.GetViewMatrix();
         view.ViewProjection = view.View
             * Camera.GetProjectionMatrix(XM_PIDIV4, AspectRatio(), 1.0f, 4000.0f);
         {
             using namespace DirectX;
             const FSimdVector light = XMVector3Normalize(XMVectorSet(0.3f, 0.8f, 0.4f, 0.0f));
             XMStoreFloat3(&view.LightDirection, light);
         }
         view.TimeSeconds = ElapsedTime;
         view.ViewportWidth = static_cast<float>(mClientWidth);
         view.ViewportHeight = static_cast<float>(mClientHeight);
         view.FillMode = ViewFillMode;

         Renderer->UpdateView(view);
         Renderer->RenderScene(Scene, mCommandList.Get());

         mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
             CurrentBackBuffer(),
             D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

         ID3D12CommandList* commandLists[] = { mCommandList.Get() };
         mFrameFenceValues[frameIndex] =
             mQueueManager->GetGraphicsQueue()->ExecuteCommandLists(
                 _countof(commandLists), commandLists);

         // Present the frame
         D3D12_THROW_IF_FAILED(mSwapChain->Present(0, 0));
         mCurrentBackBuffer = mSwapChain->GetCurrentBackBufferIndex();
     }

     // ---- 应用快捷键（相机漫游由 D3DApp 统一处理）----
     virtual void OnKeyDown(WPARAM key) override
     {
         switch (key)
         {
         case 'F':
             // 只切换 View Mode；渲染时由 Mesh Pass Processor 按 view.FillMode 选预建的 PSO，作用于全部物体。
             ViewFillMode = (ViewFillMode == EViewFillMode::Wireframe)
                 ? EViewFillMode::Solid
                 : EViewFillMode::Wireframe;
             break;
         case '+':
         case '=':
             GroundMaterial->TessellationFactor = MathHelper::Min(GroundMaterial->TessellationFactor + 1.0f, 16.0f);
             break;
         case '-':
         case '_':
             GroundMaterial->TessellationFactor = MathHelper::Max(GroundMaterial->TessellationFactor - 1.0f, 1.0f);
             break;
         default:
             break;
         }
     }

 private:
    bool InitializeRenderer()
    {
        ID3D12Device* device = mDevice.Get();
        ID3D12GraphicsCommandList* commandList = mCommandList.Get();

        Renderer = std::make_unique<FRenderer>();
        Renderer->Initialize(
            device,
            mDirectCmdListAlloc[0].Get(),   // 仅初始化期使用；FlushInitUploads 后 GPU 已空闲
            commandList,
            mQueueManager->GetGraphicsQueue(),
            mBackBufferFormat, mDepthStencilFormat);

        FShaderManager& shaderManager = Renderer->GetShaderManager();
        BindlessAllocator& bindless = Renderer->GetBindlessAllocator();
        FResourceUploadContext& uploadContext = Renderer->GetUploadContext();
        const FVertexFactory* staticMeshFactory = Renderer->GetStaticMeshVertexFactory();

        // ---- 城市网格（occcity.bin：BC1 纹理 + VB + IB 连续存储，布局见 occcity.h）----
        Microsoft::WRL::ComPtr<ID3DBlob> assetBlob = d3dUtil::LoadBinary(L"occcity.bin");
        const uint8_t* assetData = static_cast<const uint8_t*>(assetBlob->GetBufferPointer());
        const size_t assetSize = assetBlob->GetBufferSize();
        APP_CHECK_MSG(assetSize >= static_cast<size_t>(SampleAssets::IndexDataOffset) + SampleAssets::IndexDataSize,
            "occcity.bin smaller than expected (vertex/index layout mismatch)");

        FStaticMesh cityStaticMesh;
        cityStaticMesh.Vertices.resize(SampleAssets::VertexDataSize / SampleAssets::StandardVertexStride);
        std::memcpy(cityStaticMesh.Vertices.data(),
            assetData + SampleAssets::VertexDataOffset, SampleAssets::VertexDataSize);
        cityStaticMesh.Indices.resize(SampleAssets::IndexDataSize / sizeof(uint32_t));
        std::memcpy(cityStaticMesh.Indices.data(),
            assetData + SampleAssets::IndexDataOffset, SampleAssets::IndexDataSize);
        {
            const SampleAssets::DrawParameters& draw = SampleAssets::Draws[0];
            FStaticMeshSection section = {};
            section.Name = "City";
            section.MaterialIndex = 0;
            section.IndexStart = draw.IndexStart;
            section.IndexCount = draw.IndexCount;
            section.VertexBase = draw.VertexBase;
            cityStaticMesh.Sections.push_back(section);
        }
        cityStaticMesh.RecomputeBounds();
        CityMesh = std::make_unique<FMeshRenderData>(
            CreateMeshRenderData(device, commandList, cityStaticMesh, staticMeshFactory, uploadContext));

        const SampleAssets::TextureResource& cityTextureInfo = SampleAssets::Textures[0];
        CityTexture = std::make_unique<FTextureResource>(
            CreateTextureFromBC1(
                device, commandList,
                assetData,                     // BC1 数据在文件头 0 偏移处
                cityTextureInfo.Width, cityTextureInfo.Height,
                cityTextureInfo.Data[0].Pitch,
                bindless, uploadContext));

        // ---- 材质：城市（VS+PS 基线）----
        CityMaterial = std::make_unique<FMaterial>();
        CityMaterial->VertexShader = shaderManager.LoadShader("shader_basepass_vert");
        CityMaterial->PixelShader = shaderManager.LoadShader("shader_basepass_pixel");
        CityMaterial->DiffuseSrvArrayIndex = CityTexture->SrvArrayIndex;
        CityMaterial->Rasterizer = ERasterizerMode::SolidCullNone;
        Renderer->RegisterMaterial(CityMaterial.get(), staticMeshFactory);

        // ---- 地面（VS+HS+DS+PS Tessellation；Wireframe 由全局 View Mode 控制，不另建材质）----
        GroundTexture = std::make_unique<FTextureResource>(
            CreateCheckerTexture(device, commandList, 256, 256, 32, bindless, uploadContext));

        GroundMaterial = std::make_unique<FMaterial>();
        GroundMaterial->VertexShader = shaderManager.LoadShader("shader_tess_vert");
        GroundMaterial->HullShader = shaderManager.LoadShader("shader_tess_hull");
        GroundMaterial->DomainShader = shaderManager.LoadShader("shader_tess_domain");
        GroundMaterial->PixelShader = shaderManager.LoadShader("shader_basepass_pixel");
        GroundMaterial->Pipeline = EMaterialShaderPipeline::Tessellated;
        GroundMaterial->DiffuseSrvArrayIndex = GroundTexture->SrvArrayIndex;
        GroundMaterial->TessellationFactor = 4.0f;
        GroundMaterial->Rasterizer = ERasterizerMode::SolidCullNone;
        Renderer->RegisterMaterial(GroundMaterial.get(), staticMeshFactory);

        GroundMesh = std::make_unique<FMeshRenderData>(
            CreateMeshRenderData(device, commandList, GroundStaticMesh, staticMeshFactory, uploadContext));

        // ---- 场景：地面 + 4x4 城市实例网格（同网格同材质 -> 实例合并验证）----
        Scene.AddPrimitive(GroundMesh.get(), GroundMaterial.get(), MathHelper::Identity4x4());

        {
            using namespace DirectX;
            // 网格间距按城市包围盒自适应，避免魔法数字。
            const FVector3 extent = FVector3(
                cityStaticMesh.BoundsMax.x - cityStaticMesh.BoundsMin.x,
                cityStaticMesh.BoundsMax.y - cityStaticMesh.BoundsMin.y,
                cityStaticMesh.BoundsMax.z - cityStaticMesh.BoundsMin.z);
            const float spacing = MathHelper::Max(extent.x, extent.z) * 1.25f;
            const float gridHalfExtent = spacing * 1.5f;

            for (uint32_t row = 0; row < 4; ++row)
            {
                for (uint32_t column = 0; column < 4; ++column)
                {
                    const float x = -gridHalfExtent + column * spacing;
                    const float z = -gridHalfExtent + row * spacing;
                    const FSimdMatrix translation = XMMatrixTranslation(x, 0.0f, z);
                    FMatrix4x4 localToWorld = {};
                    XMStoreFloat4x4(&localToWorld, translation);
                    Scene.AddPrimitive(CityMesh.get(), CityMaterial.get(), localToWorld);
                }
            }
        }

        // ---- 场景：Box


        // 初始化期上传：执行 + CPU 等待，此后默认堆资源可安全使用。
        Renderer->FlushInitUploads();

        // 自适应取景（渲染验证用）：相机摆在 4x4 城市网格中心正前方（默认朝向 -Z 水平看），
        // 距离按城市包围盒推算（spacing = extent*1.25，网格半径 = spacing*1.5，再留约 1.3 倍余量），
        // 保证全部城市必然落入视锥中央——画面应为：黑底 + 中央城市群 + 棋盘格地面。
        {
            const float cityExtent = MathHelper::Max(
                cityStaticMesh.BoundsMax.x - cityStaticMesh.BoundsMin.x,
                cityStaticMesh.BoundsMax.z - cityStaticMesh.BoundsMin.z);
            const float gridRadius = cityExtent * 1.25f * 1.5f;
            const float viewDistance = gridRadius * 1.8f;
            Camera.Init(FVector3(0.0f, viewDistance * 0.45f, viewDistance));
            Camera.SetMoveSpeed(40.0f);
            // Alt 环绕枢轴落在城市网格中心上方（相机正前方 viewDistance 处）。
            CameraController.GetSettings().OrbitDistance = viewDistance;
        }
        return true;
    }

    // ---- 渲染器/场景资源 ----
    std::unique_ptr<FRenderer> Renderer;
    FScene Scene;

    std::unique_ptr<FMeshRenderData> CityMesh;
    std::unique_ptr<FTextureResource> CityTexture;
    std::unique_ptr<FMaterial> CityMaterial;

    std::unique_ptr<FMeshRenderData> GroundMesh;
    std::unique_ptr<FTextureResource> GroundTexture;
    std::unique_ptr<FMaterial> GroundMaterial;

    float ElapsedTime = 0.0f;
    uint32_t FrameCounter = 0;
    EViewFillMode ViewFillMode = EViewFillMode::Solid;    // 全局 View Mode（F 键切换）
};

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
#if defined(_DEBUG) || defined(DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        TestApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;
        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBoxA(nullptr, e.ToString().c_str(), "HR Failed", MB_OK);
        return 0;
    }
}
