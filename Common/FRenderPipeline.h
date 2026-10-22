#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "RHIInterfaces.h"


// A lightweight, configurable render pipeline framework (RHI-agnostic layer).
// - Register root signatures and pipeline states by name.
// - Execute an ordered list of passes.
//
// Concrete RHI implementations live under `RHI/*` (e.g. DX12RHI).
class FRenderPipeline
{
public:
    using RootSignatureName = std::wstring;
    using GraphicsPipelineName = std::wstring;
    using ComputePipelineName = std::wstring;

    struct FRenderContext
    {
        IRHICommandList* Cmd = nullptr;

        // External frame resources.
        std::shared_ptr<IRHITexture2D> BackBuffer;
        FRHICpuDescriptorHandle BackBufferRTV = {};

        std::shared_ptr<IRHITexture2D> DepthStencil;
        FRHICpuDescriptorHandle DSV = {};

        FRHIViewport Viewport = {};
        FRHIRect Scissor = {};

        IRHIDescriptorHeap* CbvSrvUavHeap = nullptr;
        IRHIDescriptorHeap* SamplerHeap = nullptr;

        void* UserData = nullptr; // optional, for RHI-specific passes.
    };

    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;
        virtual const wchar_t* GetName() const = 0;
        virtual void Execute(FRenderPipeline& Pipeline, FRenderContext& Context) = 0;
    };

public:
    explicit FRenderPipeline(std::shared_ptr<IRHIDevice> Device);

    // Registries
    void RegisterRootSignature(const RootSignatureName& Name, const FRHIRootSignatureDesc& Desc);
    void RegisterGraphicsPipeline(const GraphicsPipelineName& Name, const FRHIGraphicsPipelineDesc& Desc);
    void RegisterComputePipeline(const ComputePipelineName& Name, const FRHIComputePipelineDesc& Desc);

    std::shared_ptr<IRHIRootSignature> GetRootSignature(const RootSignatureName& Name) const;
    std::shared_ptr<IRHIPipelineState> GetGraphicsPipeline(const GraphicsPipelineName& Name) const;
    std::shared_ptr<IRHIPipelineState> GetComputePipeline(const ComputePipelineName& Name) const;

    // Resource registry (minimal): named textures with simple state tracking.
    void RegisterTexture2D(const std::wstring& Name, const FRHITexture2DDesc& Desc, ERHIResourceState InitialState, const FRHIClearColor* ClearColor = nullptr);
    std::shared_ptr<IRHITexture2D> GetTexture2D(const std::wstring& Name) const;
    ERHIResourceState GetTextureState(const std::wstring& Name) const;
    void TransitionTexture(IRHICommandList* Cmd, const std::wstring& Name, ERHIResourceState NewState);

    // Pass chain (linear for now)
    void AddPass(std::unique_ptr<IRenderPass> Pass);
    void ClearPasses();
    void Execute(FRenderContext& Context);

private:
    struct FNamedTexture
    {
        std::shared_ptr<IRHITexture2D> Texture;
        ERHIResourceState State = ERHIResourceState::Common;
    };

    std::shared_ptr<IRHIDevice> m_device;

    std::unordered_map<std::wstring, std::shared_ptr<IRHIRootSignature>> m_rootSignatures;
    std::unordered_map<std::wstring, std::shared_ptr<IRHIPipelineState>> m_graphicsPipelines;
    std::unordered_map<std::wstring, std::shared_ptr<IRHIPipelineState>> m_computePipelines;

    std::unordered_map<std::wstring, FNamedTexture> m_textures;

    std::vector<std::unique_ptr<IRenderPass>> m_passes;
};
