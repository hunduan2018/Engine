// FGraphicsPSOCache.h
// 图形 PSO 缓存：以无指针 POD 描述为键创建/复用 ID3D12PipelineState。
// PSO 只在材质注册（初始化期）时创建，任何逐帧路径都不产生 PSO。
// 预留：描述中含 AS/MS 着色器字段（第二阶段 Mesh Shading；v1 恒为 Invalid，断言拦截）。

#pragma once

#include "RendererTypes.h"
#include "FShaderManager.h"
#include "FVertexFactory.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

// PSO 状态键：全部为值类型（句柄/枚举/格式），可直接哈希与比较。
struct FGraphicsPipelineStateDesc
{
    // 便捷构造（对齐 UE5 FGraphicsPipelineStateInitializer 的"位置参数 + 默认值"形态）：
    // 必填 = 顶点/像素着色器 + 顶点工厂；常用状态带默认；曲面细分 HS/DS 成对传入。
    // 输出格式（RTV/DSV/Sample）不在此——由 FRenderer 按当前输出布局填充。
    FGraphicsPipelineStateDesc(
        FShaderHandle inVertexShader,
        FShaderHandle inPixelShader,
        FVertexFactoryHandle inVertexFactory,
        FShaderHandle inHullShader = InvalidShaderHandle,
        FShaderHandle inDomainShader = InvalidShaderHandle,
        EBlendMode inBlend = EBlendMode::Opaque,
        ERasterizerMode inRasterizer = ERasterizerMode::SolidCullNone,
        EDepthMode inDepth = EDepthMode::ReadWrite,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE inTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    // 保留聚合语义（渲染器仍可按需逐字段填充/微调构造结果）。
    FGraphicsPipelineStateDesc() = default;

    FShaderHandle VertexShader = InvalidShaderHandle;
    FShaderHandle PixelShader = InvalidShaderHandle;
    FShaderHandle HullShader = InvalidShaderHandle;        // Invalid => 无曲面细分
    FShaderHandle DomainShader = InvalidShaderHandle;
    // 预留（第二阶段 Mesh Shading）：参与哈希/比较，但 v1 必须恒为 Invalid。
    FShaderHandle AmplificationShader = InvalidShaderHandle;
    FShaderHandle MeshShader = InvalidShaderHandle;

    FVertexFactoryHandle VertexFactory = InvalidVertexFactoryHandle;    // 布局身份（注册表句柄）
    EBlendMode Blend = EBlendMode::Opaque;
    ERasterizerMode Rasterizer = ERasterizerMode::SolidCullNone;
    EDepthMode Depth = EDepthMode::ReadWrite;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    uint8_t NumRenderTargets = 1;
    DXGI_FORMAT RTVFormats[8] = {};
    DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;    // 由渲染器初始化时传入（来自 D3DApp）
    uint32_t SampleCount = 1;
    uint32_t SampleMask = UINT_MAX;

    uint64_t ComputeHash() const;
    bool operator==(const FGraphicsPipelineStateDesc& other) const;
};

class FGraphicsPSOCache
{
public:
    // rootSignature 供所有 PSO 共享（本项目 v1 全局唯一根签名）。
    // rtvFormat/dsvFormat 来自 D3DApp 的后台缓冲/深度格式，缓存内不硬编码。
    void Initialize(ID3D12Device* device, FShaderManager* shaderManager,
                    ID3D12RootSignature* rootSignature,
                    DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

    // 取或建 PSO；仅在初始化期（材质注册）调用。
    // vertexFactory 提供输入元素表；其 Handle 必须与 desc.VertexFactory 一致
    //（键存句柄做身份，布局从工厂取——对齐 UE5"声明进键、数组从声明取"）。
    FPSOHandle GetOrCreate(const FGraphicsPipelineStateDesc& desc,
                           const FVertexFactory* vertexFactory);

    // 句柄转 PSO；提交器逐命令调用，O(1)。
    ID3D12PipelineState* GetPipelineState(FPSOHandle handle) const;

private:
    ID3D12Device* Device = nullptr;
    FShaderManager* ShaderManager = nullptr;
    ID3D12RootSignature* RootSignature = nullptr;
    DXGI_FORMAT RtvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT DsvFormat = DXGI_FORMAT_UNKNOWN;

    std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> PipelineStates;
    std::vector<FGraphicsPipelineStateDesc> CacheDescs;
    // 哈希桶 + 全字段相等校验，防哈希碰撞。
    std::unordered_map<uint64_t, std::vector<FPSOHandle>> CacheBuckets;
};
