// FMaterial.h
// CPU 侧材质描述（纯数据，不含设备状态）。
// 绑定映射（v1 固定）：b1 根 CBV <- FMaterialConstants；纹理走 bindless（存数组索引）。

#pragma once

#include "RendererTypes.h"
#include "MathTypes.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

// 写入常量上传环、供 b1 使用的 GPU 镜像。字段须与 shader_common.hlsli 的 FMaterialConstants 一致。
struct FMaterialConstants
{
    FVector4 BaseColor;
    uint32_t DiffuseSrvIndex;
    float TessellationFactor;
    float Pad0;
    float Pad1;
};

class FMaterial
{
public:
    // 句柄 0 是合法 PSO，未注册的槽必须显式置为 Invalid，否则漏建 PSO 无法被断言拦截。
    FMaterial()
    {
        std::fill_n(&PipelineStates[0][0], std::size(PipelineStates) * std::size(PipelineStates[0]), InvalidPSOHandle);
    }

    FShaderHandle VertexShader = InvalidShaderHandle;
    FShaderHandle PixelShader = InvalidShaderHandle;
    FShaderHandle HullShader = InvalidShaderHandle;     // Invalid => 非曲面细分
    FShaderHandle DomainShader = InvalidShaderHandle;

    EMaterialShaderPipeline Pipeline = EMaterialShaderPipeline::Default;
    ERasterizerMode Rasterizer = ERasterizerMode::SolidCullNone;

    uint32_t DiffuseSrvArrayIndex = 0;                  // g_BindlessTextures[] 下标
    FVector4 BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float TessellationFactor = 4.0f;                    // 由 ConstantHS 读取

    // 注册时由 FRenderer 填充：每 Mesh Pass × View Fill Mode 预解析的 PSO 句柄 + 排序用材质 ID。
    FPSOHandle PipelineStates[static_cast<size_t>(EMeshPass::Count)][static_cast<size_t>(EViewFillMode::Count)];
    uint16_t SortId = 0;

    bool IsTessellated() const { return Pipeline == EMaterialShaderPipeline::Tessellated; }
};
