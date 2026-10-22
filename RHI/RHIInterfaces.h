#pragma once

#include <cstdint>
#include <memory>
#include <string>


#include "RHICommon.h"

// NOTE: This header must stay RHI-agnostic (no D3D12/Vulkan/etc includes).

class IRHIObject
{
public:
    virtual ~IRHIObject() = default;
};

class IRHIRootSignature : public IRHIObject
{
public:
    ~IRHIRootSignature() override = default;
};

class IRHIPipelineState : public IRHIObject
{
public:
    ~IRHIPipelineState() override = default;
};

class IRHIResource : public IRHIObject
{
public:
    ~IRHIResource() override = default;
};

class IRHITexture2D : public IRHIResource
{
public:
    ~IRHITexture2D() override = default;
};

class IRHIDescriptorHeap : public IRHIObject
{
public:
    ~IRHIDescriptorHeap() override = default;
    virtual FRHIGpuDescriptorHandle GetGPUDescriptorHandleForHeapStart() const = 0;
};

class IRHICommandList : public IRHIObject
{
public:
    ~IRHICommandList() override = default;

    // Optional escape hatch for RHI-specific passes.
    virtual void* GetNativeHandle() const = 0;

    virtual void RSSetViewports(uint32_t Count, const FRHIViewport* Viewports) = 0;
    virtual void RSSetScissorRects(uint32_t Count, const FRHIRect* Rects) = 0;

    virtual void ResourceBarrier(IRHIResource* Resource, ERHIResourceState Before, ERHIResourceState After) = 0;

    virtual void SetPipelineState(IRHIPipelineState* PSO) = 0;
    virtual void SetGraphicsRootSignature(IRHIRootSignature* RootSig) = 0;

    virtual void SetDescriptorHeaps(IRHIDescriptorHeap* CbvSrvUavHeap, IRHIDescriptorHeap* SamplerHeap) = 0;
    virtual void SetGraphicsRootDescriptorTable(uint32_t RootParameterIndex, FRHIGpuDescriptorHandle BaseDescriptor) = 0;

    virtual void OMSetRenderTargets(uint32_t NumRTVs, const FRHICpuDescriptorHandle* RTVs, bool bRTVsSingleHandleToDescriptorRange,
        const FRHICpuDescriptorHandle* DSV) = 0;

    virtual void ClearRenderTargetView(FRHICpuDescriptorHandle RTV, const float ColorRGBA[4]) = 0;
    virtual void ClearDepthStencilView(FRHICpuDescriptorHandle DSV, float Depth, uint8_t Stencil) = 0;

    virtual void IASetPrimitiveTopology(ERHIPrimitiveTopology Topology) = 0;
    virtual void DrawInstanced(uint32_t VertexCountPerInstance, uint32_t InstanceCount, uint32_t StartVertexLocation, uint32_t StartInstanceLocation) = 0;

    virtual void ExecuteBundle(IRHICommandList* Bundle) = 0;
};

struct FRHIVertexElement
{
    const char* SemanticName = nullptr;
    uint32_t SemanticIndex = 0;
    ERHIFormat Format = ERHIFormat::Unknown;
    uint32_t InputSlot = 0;
    uint32_t AlignedByteOffset = 0;
    bool bPerInstance = false;
    uint32_t InstanceDataStepRate = 0;
};

struct FRHIVertexLayout
{
    const FRHIVertexElement* Elements = nullptr;
    uint32_t NumElements = 0;
};

struct FRHIRasterizerStateDesc
{
    ERHICullMode CullMode = ERHICullMode::Back;
};

struct FRHIDepthStencilStateDesc
{
    bool bDepthEnable = true;
    bool bStencilEnable = false;
};

struct FRHIBlendStateDesc
{
    bool bEnable = false;
};

struct FRHIRootSignatureDesc
{
    // Matches the existing sample binding model:
    //  - Param0: SRV descriptor table (space0 + space1)
    //  - Param1: Sampler descriptor table
    //  - Param2: CBV descriptor table (vertex)
    //  - Param3: Root constants (pixel)
    uint32_t SrvCountSpace0 = 5000;
    uint32_t SrvCountSpace1 = 5000;
    uint32_t SamplerCount = 4080;
    uint32_t CbvCount = 1;

    bool bAllowInputAssemblerInputLayout = true;
};

struct FRHIGraphicsPipelineDesc
{
    // Shader bytecode paths (.cso).
    // NOTE: Loading/compilation is RHI implementation specific.
    std::wstring VSPath;
    std::wstring PSPath;

    std::wstring RootSignatureName;

    FRHIVertexLayout VertexLayout;

    ERHIFormat RTVFormat = ERHIFormat::R8G8B8A8_UNORM;
    ERHIFormat DSVFormat = ERHIFormat::D32_FLOAT;

    FRHIRasterizerStateDesc Rasterizer;
    FRHIBlendStateDesc Blend;
    FRHIDepthStencilStateDesc DepthStencil;

    ERHIPrimitiveTopologyType TopologyType = ERHIPrimitiveTopologyType::Triangle;
    uint32_t SampleMask = 0xFFFFFFFFu;
    uint32_t SampleCount = 1;
};

struct FRHIComputePipelineDesc
{
    std::wstring CSPath;
    std::wstring RootSignatureName;
};

struct FRHITexture2DDesc
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    ERHIFormat Format = ERHIFormat::R8G8B8A8_UNORM;
    bool bAllowRenderTarget = true;
    uint32_t MipLevels = 1;
    uint32_t SampleCount = 1;
};

class IRHIDevice : public IRHIObject
{
public:
    ~IRHIDevice() override = default;

    virtual std::shared_ptr<IRHIRootSignature> CreateRootSignature(const FRHIRootSignatureDesc& Desc) = 0;
    virtual std::shared_ptr<IRHIPipelineState> CreateGraphicsPipeline(const FRHIGraphicsPipelineDesc& Desc, IRHIRootSignature* RootSig) = 0;
    virtual std::shared_ptr<IRHIPipelineState> CreateComputePipeline(const FRHIComputePipelineDesc& Desc, IRHIRootSignature* RootSig) = 0;

    virtual std::shared_ptr<IRHITexture2D> CreateTexture2D(const FRHITexture2DDesc& Desc, ERHIResourceState InitialState, const FRHIClearColor* ClearColor) = 0;
};
