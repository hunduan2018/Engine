#include "../src/stdafx.h"
#include "FRenderPipeline.h"

#include "../src/DXSampleHelper.h"


FRenderPipeline::FRenderPipeline(std::shared_ptr<IRHIDevice> Device)
    : m_device(std::move(Device))
{
}

void FRenderPipeline::RegisterRootSignature(const RootSignatureName& Name, const FRHIRootSignatureDesc& Desc)
{
    m_rootSignatures[Name] = m_device->CreateRootSignature(Desc);
}

void FRenderPipeline::RegisterGraphicsPipeline(const GraphicsPipelineName& Name, const FRHIGraphicsPipelineDesc& Desc)
{
    auto rs = GetRootSignature(Desc.RootSignatureName);
    if (!rs)
    {
        ThrowIfFailed(E_FAIL);
    }

    m_graphicsPipelines[Name] = m_device->CreateGraphicsPipeline(Desc, rs.get());
}

void FRenderPipeline::RegisterComputePipeline(const ComputePipelineName& Name, const FRHIComputePipelineDesc& Desc)
{
    auto rs = GetRootSignature(Desc.RootSignatureName);
    if (!rs)
    {
        ThrowIfFailed(E_FAIL);
    }

    m_computePipelines[Name] = m_device->CreateComputePipeline(Desc, rs.get());
}

std::shared_ptr<IRHIRootSignature> FRenderPipeline::GetRootSignature(const RootSignatureName& Name) const
{
    const auto it = m_rootSignatures.find(Name);
    return (it != m_rootSignatures.end()) ? it->second : nullptr;
}

std::shared_ptr<IRHIPipelineState> FRenderPipeline::GetGraphicsPipeline(const GraphicsPipelineName& Name) const
{
    const auto it = m_graphicsPipelines.find(Name);
    return (it != m_graphicsPipelines.end()) ? it->second : nullptr;
}

std::shared_ptr<IRHIPipelineState> FRenderPipeline::GetComputePipeline(const ComputePipelineName& Name) const
{
    const auto it = m_computePipelines.find(Name);
    return (it != m_computePipelines.end()) ? it->second : nullptr;
}

void FRenderPipeline::RegisterTexture2D(const std::wstring& Name, const FRHITexture2DDesc& Desc, ERHIResourceState InitialState, const FRHIClearColor* ClearColor)
{
    FNamedTexture named;
    named.State = InitialState;
    named.Texture = m_device->CreateTexture2D(Desc, InitialState, ClearColor);
    m_textures[Name] = std::move(named);
}

std::shared_ptr<IRHITexture2D> FRenderPipeline::GetTexture2D(const std::wstring& Name) const
{
    const auto it = m_textures.find(Name);
    return (it != m_textures.end()) ? it->second.Texture : nullptr;
}

ERHIResourceState FRenderPipeline::GetTextureState(const std::wstring& Name) const
{
    const auto it = m_textures.find(Name);
    return (it != m_textures.end()) ? it->second.State : ERHIResourceState::Common;
}

void FRenderPipeline::TransitionTexture(IRHICommandList* Cmd, const std::wstring& Name, ERHIResourceState NewState)
{
    auto it = m_textures.find(Name);
    if (it == m_textures.end() || !it->second.Texture)
    {
        return;
    }

    auto& tex = it->second;
    if (tex.State == NewState)
    {
        return;
    }

    Cmd->ResourceBarrier(tex.Texture.get(), tex.State, NewState);
    tex.State = NewState;
}

void FRenderPipeline::AddPass(std::unique_ptr<IRenderPass> Pass)
{
    m_passes.emplace_back(std::move(Pass));
}

void FRenderPipeline::ClearPasses()
{
    m_passes.clear();
}

void FRenderPipeline::Execute(FRenderContext& Context)
{
    for (const auto& Pass : m_passes)
    {
        Pass->Execute(*this, Context);
    }
}
