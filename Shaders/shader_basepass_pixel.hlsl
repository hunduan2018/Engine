// shader_basepass_pixel.hlsl
// BasePass 像素着色器：bindless 纹理采样 + 单方向光 Lambert + 环境光。
// 纹理索引来自材质常量（b1），同一 draw 内 uniform，SM5.1 下合法。

#include "shader_common.hlsli"

float4 PSMain(FPixelInput input) : SV_TARGET
{
    // 世界空间法线（透视下插值不完全归一，重新归一化）。
    const float3 normal = normalize(input.WorldNormal);
    const float3 lightDirection = normalize(g_LightDirection.xyz);

    const float4 diffuse = g_BindlessTextures[g_DiffuseSrvIndex].Sample(
        g_LinearWrapSampler, input.UV);

    const float lambert = saturate(dot(normal, lightDirection));
    const float3 lighting = diffuse.rgb * (0.3f + 0.7f * lambert);

    return float4(lighting * g_BaseColor.rgb, diffuse.a * g_BaseColor.a);
}
