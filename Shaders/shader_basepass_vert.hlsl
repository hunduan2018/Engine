// shader_basepass_vert.hlsl
// BasePass 顶点着色器（非曲面细分路线）：
// 按根常量实例偏移从实例缓冲取 LocalToWorld，输出裁剪位与世界法线。

#include "shader_common.hlsli"

FPixelInput VSMain(FVertexInput input, uint instanceID : SV_InstanceID)
{
    const FInstanceData instance = g_InstanceData[g_InstanceOffset + instanceID];

    const float4 worldPosition = mul(float4(input.Position, 1.0f), instance.LocalToWorld);
    const float3 worldNormal = mul(float4(input.Normal, 0.0f), instance.LocalToWorld).xyz;

    FPixelInput result;
    result.Position = mul(worldPosition, g_ViewProjection);
    result.WorldNormal = worldNormal;
    result.UV = input.UV;
    return result;
}
