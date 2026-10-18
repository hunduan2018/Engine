// shader_tess_domain.hlsl
// 域着色器：重心插值 -> 正弦位移（解析法线）-> 实例变换 -> 视口变换。
// 输出与基线 VS（shader_basepass_vert）相同的 FPixelInput，复用同一像素着色器。

#include "shader_common.hlsli"

struct FHullOutput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    uint InstanceID : TEXCOORD1;
};

struct FPatchTessFactors
{
    float EdgeTessFactor[3] : SV_TessFactor;
    float InsideTessFactor : SV_InsideTessFactor;
};

// 位移场：y = A * sin(x*k + t) * cos(z*k + t)；解析偏导推导法线。
static const float DisplaceAmplitude = 3.0f;
static const float DisplaceFrequency = 0.3f;

[domain("tri")]
FPixelInput DSMain(FPatchTessFactors factors,
                   const OutputPatch<FHullOutput, 3> patch,
                   float3 barycentric : SV_DomainLocation)
{
    // 重心插值控制点属性。
    const float3 position =
        patch[0].Position * barycentric.x
        + patch[1].Position * barycentric.y
        + patch[2].Position * barycentric.z;
    const float2 uv =
        patch[0].UV * barycentric.x
        + patch[1].UV * barycentric.y
        + patch[2].UV * barycentric.z;

    // 位移 + 解析法线。
    const float time = g_ViewOriginTime.w;
    const float k = DisplaceFrequency;
    const float sx = sin(position.x * k + time);
    const float cx = cos(position.x * k + time);
    const float sz = sin(position.z * k + time);
    const float cz = cos(position.z * k + time);

    const float3 displaced = float3(position.x, position.y + DisplaceAmplitude * sx * cz, position.z);
    const float dYdX = DisplaceAmplitude * k * cx * cz;
    const float dYdZ = -DisplaceAmplitude * k * sx * sz;
    const float3 objectNormal = normalize(float3(-dYdX, 1.0f, -dYdZ));

    // 实例变换 + 视口变换（实例 ID 由 VS 读取、经控制点直通）。
    const FInstanceData instance = g_InstanceData[g_InstanceOffset + patch[0].InstanceID];
    const float4 worldPosition = mul(float4(displaced, 1.0f), instance.LocalToWorld);
    const float3 worldNormal = mul(float4(objectNormal, 0.0f), instance.LocalToWorld).xyz;

    FPixelInput result;
    result.Position = mul(worldPosition, g_ViewProjection);
    result.WorldNormal = worldNormal;
    result.UV = uv;
    return result;
}
