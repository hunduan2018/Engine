// shader_tess_vert.hlsl
// 曲面细分路线顶点着色器：直通控制点（物体空间）。
// 实例变换与视口变换在域着色器完成——细分必须发生在物体空间。

#include "shader_common.hlsli"

struct FHullInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    uint InstanceID : TEXCOORD1;    // VS 读取 SV_InstanceID 后以自定义语义传递
                                    //（DS 不能直接读 SV_InstanceID）
};

FHullInput VSMain(FVertexInput input, uint instanceID : SV_InstanceID)
{
    FHullInput result;
    result.Position = input.Position;
    result.Normal = input.Normal;
    result.UV = input.UV;
    result.InstanceID = instanceID;
    return result;
}
