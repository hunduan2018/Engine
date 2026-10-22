#pragma once

#include <string>
#include <utility>

#include <d3d12.h>

#include "FRenderPipeline.h"
#include "RHIInterfaces.h"
#include "DX12RHICommandList.h"
#include "DX12RHIPipelineObjects.h"

#include "FrameResource.h"

// DX12-specific city geometry pass (bridges to existing FrameResource bundle path).
class FDX12CityGeometryPass : public FRenderPipeline::IRenderPass
{
public:
    struct FInputs
    {
        FrameResource* Frame = nullptr;
        uint32_t FrameResourceIndex = 0;
        uint32_t NumIndices = 0;
        void* IBV = nullptr; // D3D12_INDEX_BUFFER_VIEW*
        void* VBV = nullptr; // D3D12_VERTEX_BUFFER_VIEW*
        ID3D12DescriptorHeap* CbvSrvUavHeap = nullptr;
        ID3D12DescriptorHeap* SamplerHeap = nullptr;
        uint32_t CbvSrvUavDescriptorSize = 0;
        bool bUseBundles = true;
    };

    explicit FDX12CityGeometryPass(FRenderPipeline::GraphicsPipelineName GraphicsPipeline,
        FRenderPipeline::RootSignatureName RootSignature)
        : m_graphicsPipeline(std::move(GraphicsPipeline))
        , m_rootSignature(std::move(RootSignature))
    {
    }


    const wchar_t* GetName() const override { return L"DX12CityGeometry"; }

    void Execute(FRenderPipeline& Pipeline, FRenderPipeline::FRenderContext& Context) override
    {
        auto pso = Pipeline.GetGraphicsPipeline(m_graphicsPipeline);
        auto rootSig = Pipeline.GetRootSignature(m_rootSignature);

        Context.Cmd->SetPipelineState(pso.get());
        Context.Cmd->SetGraphicsRootSignature(rootSig.get());

        auto* inputs = static_cast<FInputs*>(Context.UserData);
        if (!inputs || !inputs->Frame)
        {
            return;
        }


        auto* nativeCmd = static_cast<ID3D12GraphicsCommandList*>(Context.Cmd->GetNativeHandle());
        if (!nativeCmd)
        {
            return;
        }

        PIXBeginEvent(nativeCmd, 0, L"Draw cities");

        // IMPORTANT: When executing a bundle, the direct command list must have the same
        // descriptor heaps bound as the bundle expects, otherwise D3D12 debug layer will
        // raise EXECUTE_BUNDLE_DESCRIPTOR_HEAP_MISMATCH.
        Context.Cmd->SetDescriptorHeaps(Context.CbvSrvUavHeap, Context.SamplerHeap);

        if (inputs->bUseBundles)
        {
            nativeCmd->ExecuteBundle(inputs->Frame->m_GraphicCommandList.Get());
        }
        else
        {
            auto* dxRoot = dynamic_cast<FDX12RHIRootSignature*>(rootSig.get());
            if (!dxRoot)
            {
                PIXEndEvent(nativeCmd);
                return;
            }

            inputs->Frame->PopulateCommandList(
                nativeCmd,
                inputs->FrameResourceIndex,
                inputs->NumIndices,
                static_cast<D3D12_INDEX_BUFFER_VIEW*>(inputs->IBV),
                static_cast<D3D12_VERTEX_BUFFER_VIEW*>(inputs->VBV),
                inputs->CbvSrvUavHeap,
                inputs->CbvSrvUavDescriptorSize,
                inputs->SamplerHeap,
                dxRoot->GetNative());
        }


        PIXEndEvent(nativeCmd);
    }

private:
    FRenderPipeline::GraphicsPipelineName m_graphicsPipeline;
    FRenderPipeline::RootSignatureName m_rootSignature;
};


class FDX12BeginBackbufferPass : public FRenderPipeline::IRenderPass
{
public:
    const wchar_t* GetName() const override { return L"BeginBackbuffer"; }

    void Execute(FRenderPipeline& Pipeline, FRenderPipeline::FRenderContext& Context) override
    {
        (void)Pipeline;

        Context.Cmd->RSSetViewports(1, &Context.Viewport);
        Context.Cmd->RSSetScissorRects(1, &Context.Scissor);

        Context.Cmd->ResourceBarrier(Context.BackBuffer.get(), ERHIResourceState::Present, ERHIResourceState::RenderTarget);

        Context.Cmd->OMSetRenderTargets(1, &Context.BackBufferRTV, false, Context.DepthStencil ? &Context.DSV : nullptr);
        Context.Cmd->ClearRenderTargetView(Context.BackBufferRTV, ClearColor);
        if (Context.DepthStencil)
        {
            Context.Cmd->ClearDepthStencilView(Context.DSV, ClearDepth, ClearStencil);
        }
    }

public:
    float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    float ClearDepth = 1.0f;
    uint8_t ClearStencil = 0;
};

class FDX12BeginTextureRenderTargetPass : public FRenderPipeline::IRenderPass
{
public:
    FDX12BeginTextureRenderTargetPass(std::wstring TextureName, FRHICpuDescriptorHandle RTV)
        : m_textureName(std::move(TextureName))
        , m_rtv(RTV)
    {
    }

    const wchar_t* GetName() const override { return L"BeginTextureRT"; }

    void Execute(FRenderPipeline& Pipeline, FRenderPipeline::FRenderContext& Context) override
    {
        Context.Cmd->RSSetViewports(1, &Context.Viewport);
        Context.Cmd->RSSetScissorRects(1, &Context.Scissor);

        Pipeline.TransitionTexture(Context.Cmd, m_textureName, ERHIResourceState::RenderTarget);

        Context.Cmd->OMSetRenderTargets(1, &m_rtv, false, Context.DepthStencil ? &Context.DSV : nullptr);
        Context.Cmd->ClearRenderTargetView(m_rtv, ClearColor);

        if (Context.DepthStencil)
        {
            Context.Cmd->ClearDepthStencilView(Context.DSV, ClearDepth, ClearStencil);
        }
    }

public:
    float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    float ClearDepth = 1.0f;
    uint8_t ClearStencil = 0;

private:
    std::wstring m_textureName;
    FRHICpuDescriptorHandle m_rtv = {};
};

class FDX12PostProcessCopyPass : public FRenderPipeline::IRenderPass
{
public:
    FDX12PostProcessCopyPass(FRenderPipeline::GraphicsPipelineName GraphicsPipeline,
        FRenderPipeline::RootSignatureName RootSignature,
        std::wstring SourceTextureName)
        : m_graphicsPipeline(std::move(GraphicsPipeline))
        , m_rootSignature(std::move(RootSignature))
        , m_sourceTextureName(std::move(SourceTextureName))
    {
    }

    const wchar_t* GetName() const override { return L"PostProcessCopy"; }

    void Execute(FRenderPipeline& Pipeline, FRenderPipeline::FRenderContext& Context) override
    {
        auto pso = Pipeline.GetGraphicsPipeline(m_graphicsPipeline);
        auto rootSig = Pipeline.GetRootSignature(m_rootSignature);

        Pipeline.TransitionTexture(Context.Cmd, m_sourceTextureName, ERHIResourceState::PixelShaderResource);

        Context.Cmd->ResourceBarrier(Context.BackBuffer.get(), ERHIResourceState::Present, ERHIResourceState::RenderTarget);

        Context.Cmd->SetPipelineState(pso.get());
        Context.Cmd->SetGraphicsRootSignature(rootSig.get());

        Context.Cmd->SetDescriptorHeaps(Context.CbvSrvUavHeap, Context.SamplerHeap);

        Context.Cmd->SetGraphicsRootDescriptorTable(0, Context.CbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
        Context.Cmd->SetGraphicsRootDescriptorTable(1, Context.SamplerHeap->GetGPUDescriptorHandleForHeapStart());

        Context.Cmd->RSSetViewports(1, &Context.Viewport);
        Context.Cmd->RSSetScissorRects(1, &Context.Scissor);
        Context.Cmd->OMSetRenderTargets(1, &Context.BackBufferRTV, false, nullptr);
        Context.Cmd->IASetPrimitiveTopology(ERHIPrimitiveTopology::TriangleList);
        Context.Cmd->DrawInstanced(3, 1, 0, 0);
    }

private:
    FRenderPipeline::GraphicsPipelineName m_graphicsPipeline;
    FRenderPipeline::RootSignatureName m_rootSignature;
    std::wstring m_sourceTextureName;
};

class FDX12EndBackbufferPass : public FRenderPipeline::IRenderPass
{
public:
    const wchar_t* GetName() const override { return L"EndBackbuffer"; }

    void Execute(FRenderPipeline& Pipeline, FRenderPipeline::FRenderContext& Context) override
    {
        (void)Pipeline;
        Context.Cmd->ResourceBarrier(Context.BackBuffer.get(), ERHIResourceState::RenderTarget, ERHIResourceState::Present);
    }
};
