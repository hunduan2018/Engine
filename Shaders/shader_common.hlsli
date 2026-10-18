// shader_common.hlsli
// 全局根签名对应的共享声明。字段布局必须与 Renderer 侧 CPU 镜像一致：
//   b0 <- FViewConstants (FRenderer.h)   b1 <- FMaterialConstants (FMaterial.h)
//   b2 <- FDrawRootConstants              t0 <- FInstanceData (FInstanceAllocator.h)
// 矩阵均为 CPU 侧转置后上传，着色器统一使用 mul(向量, 矩阵)。

// ---- b0 视图常量 ----
cbuffer FViewConstants : register(b0)
{
    float4x4 g_View;
    float4x4 g_ViewProjection;
    float4 g_ViewOriginTime;    // xyz = 视点, w = 时间秒
    float4 g_LightDirection;    // xyz = 指向光源
    float4 g_Resolution;        // xy = 视口尺寸
};

// ---- b1 材质常量 ----
cbuffer FMaterialConstants : register(b1)
{
    float4 g_BaseColor;
    uint g_DiffuseSrvIndex;     // g_BindlessTextures[] 下标（draw 内 uniform，SM5.1 合法）
    float g_TessellationFactor;
    float2 g_MaterialPad;
};

// ---- b2 绘制根常量 ----
cbuffer FDrawRootConstants : register(b2)
{
    uint g_InstanceOffset;      // 实例缓冲（t0）中的起始下标
    uint3 g_DrawReserved;       // 预留：第二阶段 MeshShading 的缓冲 SRV 索引
};

// ---- t0 实例缓冲（bindless 堆槽 0）----
struct FInstanceData
{
    float4x4 LocalToWorld;
};
StructuredBuffer<FInstanceData> g_InstanceData : register(t0, space0);

// ---- t1.. bindless 纹理数组 ----
// 有界数组（无需 /enable_unbounded_descriptor_tables）；Release 用 ps_5_1 均可编译。
Texture2D g_BindlessTextures[1024] : register(t1, space0);

// ---- 静态采样器（根签名内置）----
SamplerState g_LinearWrapSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

// ---- 顶点输入（FStaticMeshVertex，44 字节）----
struct FVertexInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
    float3 Tangent  : TANGENT;
};

// ---- 像素输入（基线路线 VS 与曲面细分 DS 共用）----
struct FPixelInput
{
    float4 Position : SV_POSITION;
    float3 WorldNormal : NORMAL;
    float2 UV       : TEXCOORD0;
};
