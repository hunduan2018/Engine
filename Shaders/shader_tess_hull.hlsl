// shader_tess_hull.hlsl
// 外壳着色器：3 控制点直通；补丁常量从材质常量读取细分因子。

#include "shader_common.hlsli"

struct FHullInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    uint InstanceID : TEXCOORD1;
};

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

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
FHullOutput HSMain(InputPatch<FHullInput, 3> patch, uint controlPointID : SV_OutputControlPointID)
{
    FHullOutput result;
    result.Position = patch[controlPointID].Position;
    result.Normal = patch[controlPointID].Normal;
    result.UV = patch[controlPointID].UV;
    result.InstanceID = patch[controlPointID].InstanceID;
    return result;
}

FPatchTessFactors ConstantHS(InputPatch<FHullInput, 3> patch, uint patchID : SV_PrimitiveID)
{
    FPatchTessFactors factors;
    const float tessFactor = max(g_TessellationFactor, 11.0f);
    factors.EdgeTessFactor[0] = tessFactor;
    factors.EdgeTessFactor[1] = tessFactor;
    factors.EdgeTessFactor[2] = tessFactor;
    factors.InsideTessFactor = tessFactor;
    return factors;
}
