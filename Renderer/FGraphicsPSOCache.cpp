// FGraphicsPSOCache.cpp

#include "FGraphicsPSOCache.h"

#include "../src/Assert.h"
#include "d3dUtil.h"
#include "d3dx12.h"

// FNV-1a 64：按键的 POD 字节流逐字段计算（避免结构体填充字节的不确定性，逐成员喂入）。
static void HashCombine(uint64_t& hash, const void* data, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 0x100000001B3ull;
    }
}

FGraphicsPipelineStateDesc::FGraphicsPipelineStateDesc(
    FShaderHandle inVertexShader,
    FShaderHandle inPixelShader,
    FVertexFactoryHandle inVertexFactory,
    FShaderHandle inHullShader,
    FShaderHandle inDomainShader,
    EBlendMode inBlend,
    ERasterizerMode inRasterizer,
    EDepthMode inDepth,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE inTopologyType)
    : VertexShader(inVertexShader)
    , PixelShader(inPixelShader)
    , HullShader(inHullShader)
    , DomainShader(inDomainShader)
    , VertexFactory(inVertexFactory)
    , Blend(inBlend)
    , Rasterizer(inRasterizer)
    , Depth(inDepth)
    , TopologyType(inTopologyType)
{
}

uint64_t FGraphicsPipelineStateDesc::ComputeHash() const
{
    uint64_t hash = 0xcbf29ce484222325ull;
    HashCombine(hash, &VertexShader, sizeof(VertexShader));
    HashCombine(hash, &PixelShader, sizeof(PixelShader));
    HashCombine(hash, &HullShader, sizeof(HullShader));
    HashCombine(hash, &DomainShader, sizeof(DomainShader));
    HashCombine(hash, &AmplificationShader, sizeof(AmplificationShader));
    HashCombine(hash, &MeshShader, sizeof(MeshShader));
    HashCombine(hash, &VertexFactory, sizeof(VertexFactory));
    HashCombine(hash, &Blend, sizeof(Blend));
    HashCombine(hash, &Rasterizer, sizeof(Rasterizer));
    HashCombine(hash, &Depth, sizeof(Depth));
    HashCombine(hash, &TopologyType, sizeof(TopologyType));
    HashCombine(hash, &NumRenderTargets, sizeof(NumRenderTargets));
    HashCombine(hash, RTVFormats, sizeof(RTVFormats));
    HashCombine(hash, &DSVFormat, sizeof(DSVFormat));
    HashCombine(hash, &SampleCount, sizeof(SampleCount));
    HashCombine(hash, &SampleMask, sizeof(SampleMask));
    return hash;
}

bool FGraphicsPipelineStateDesc::operator==(const FGraphicsPipelineStateDesc& other) const
{
    return VertexShader == other.VertexShader
        && PixelShader == other.PixelShader
        && HullShader == other.HullShader
        && DomainShader == other.DomainShader
        && AmplificationShader == other.AmplificationShader
        && MeshShader == other.MeshShader
        && VertexFactory == other.VertexFactory
        && Blend == other.Blend
        && Rasterizer == other.Rasterizer
        && Depth == other.Depth
        && TopologyType == other.TopologyType
        && NumRenderTargets == other.NumRenderTargets
        && SampleCount == other.SampleCount
        && SampleMask == other.SampleMask
        && memcmp(RTVFormats, other.RTVFormats, sizeof(RTVFormats)) == 0
        && DSVFormat == other.DSVFormat;
}

void FGraphicsPSOCache::Initialize(ID3D12Device* device, FShaderManager* shaderManager,
                                   ID3D12RootSignature* rootSignature,
                                   DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
{
    Device = device;
    ShaderManager = shaderManager;
    RootSignature = rootSignature;
    RtvFormat = rtvFormat;
    DsvFormat = dsvFormat;
}

FPSOHandle FGraphicsPSOCache::GetOrCreate(const FGraphicsPipelineStateDesc& desc,
                                          const FVertexFactory* vertexFactory)
{
    APP_CHECK(Device != nullptr);
    APP_CHECK_MSG(vertexFactory != nullptr && vertexFactory->Handle == desc.VertexFactory,
        "vertexFactory must match desc.VertexFactory (handle is the layout identity)");
    APP_CHECK_MSG(desc.AmplificationShader == InvalidShaderHandle && desc.MeshShader == InvalidShaderHandle,
        "Mesh Shading (AS+MS) pipelines are reserved for stage 2; not creatable yet");
    // 曲面细分 HS/DS 必须成对出现。
    APP_CHECK_MSG((desc.HullShader == InvalidShaderHandle) == (desc.DomainShader == InvalidShaderHandle),
        "Hull and Domain shaders must be set (or unset) together");

    const uint64_t hash = desc.ComputeHash();
    auto bucketIt = CacheBuckets.find(hash);
    if (bucketIt != CacheBuckets.end())
    {
        for (FPSOHandle handle : bucketIt->second)
        {
            if (desc == CacheDescs[handle])
            {
                return handle;
            }
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = RootSignature;    // PSO 必须携带根签名（驱动验证着色器绑定的依据）
    psoDesc.VS = ShaderManager->GetShaderBytecode(desc.VertexShader);
    psoDesc.PS = ShaderManager->GetShaderBytecode(desc.PixelShader);
    const bool bTessellated = (desc.HullShader != InvalidShaderHandle);
    if (bTessellated)
    {
        psoDesc.HS = ShaderManager->GetShaderBytecode(desc.HullShader);
        psoDesc.DS = ShaderManager->GetShaderBytecode(desc.DomainShader);
    }

    // 输入布局来自顶点工厂（构造时已汇编排序；拓扑由 TopologyType 独立表达）。
    {
        uint32_t elementCount = 0;
        psoDesc.InputLayout.pInputElementDescs = vertexFactory->GetInputElementDescs(elementCount);
        psoDesc.InputLayout.NumElements = elementCount;
    }

    // 混合状态。
    switch (desc.Blend)
    {
    case EBlendMode::Opaque:
    case EBlendMode::Masked:        // v1：片元裁剪在着色器内完成，混合状态与 Opaque 相同
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        break;
    case EBlendMode::Translucent:   // 预留
    default:
        APP_CHECK_MSG(false, "EBlendMode::Translucent is reserved; not supported yet");
        break;
    }

    // 光栅化状态。
    switch (desc.Rasterizer)
    {
    case ERasterizerMode::SolidCullBack:
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        break;
    case ERasterizerMode::SolidCullNone:
    {
        CD3DX12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rasterizer.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState = rasterizer;
        break;
    }
    case ERasterizerMode::Wireframe:
    {
        CD3DX12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizer.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState = rasterizer;
        break;
    }
    default:
        APP_CHECK_MSG(false, "Unknown ERasterizerMode");
        break;
    }

    // 深度模板状态。
    CD3DX12_DEPTH_STENCIL_DESC depthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    switch (desc.Depth)
    {
    case EDepthMode::ReadWrite:
        depthStencil.DepthEnable = TRUE;
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        break;
    case EDepthMode::ReadOnly:
        depthStencil.DepthEnable = TRUE;
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        break;
    case EDepthMode::Disabled:
        depthStencil.DepthEnable = FALSE;
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        break;
    default:
        APP_CHECK_MSG(false, "Unknown EDepthMode");
        break;
    }
    psoDesc.DepthStencilState = depthStencil;

    psoDesc.SampleMask = desc.SampleMask;
    psoDesc.PrimitiveTopologyType = desc.TopologyType;
    psoDesc.NumRenderTargets = desc.NumRenderTargets;
    for (uint8_t i = 0; i < desc.NumRenderTargets; ++i)
    {
        psoDesc.RTVFormats[i] = desc.RTVFormats[i];
    }
    psoDesc.DSVFormat = desc.DSVFormat;
    psoDesc.SampleDesc.Count = desc.SampleCount;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_THROW_IF_FAILED(Device->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pipelineState)));
    pipelineState->SetName(L"FGraphicsPSOCache PSO");

    const FPSOHandle handle = static_cast<FPSOHandle>(PipelineStates.size());
    PipelineStates.push_back(std::move(pipelineState));
    CacheDescs.push_back(desc);
    CacheBuckets[hash].push_back(handle);
    return handle;
}

ID3D12PipelineState* FGraphicsPSOCache::GetPipelineState(FPSOHandle handle) const
{
    APP_CHECK(handle < PipelineStates.size());
    return PipelineStates[handle].Get();
}
