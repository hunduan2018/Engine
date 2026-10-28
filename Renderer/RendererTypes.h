// RendererTypes.h
// 渲染架构共享的枚举、句柄与编译期常量。
// 说明：小体积的枚举/句柄按 UE "Types 头文件" 惯例集中于此（一类型一文件规则的例外）。

#pragma once

#include <cstdint>

// ---- 句柄 ----
// 句柄是各管理器内部的数组索引；0xFFFFFFFF 表示无效，创建失败/未设置时使用。
using FShaderHandle = uint32_t;
using FPSOHandle = uint32_t;
using FVertexFactoryHandle = uint32_t;   // 顶点布局身份（FRenderer 工厂注册表分配）

constexpr uint32_t InvalidHandle = 0xFFFFFFFF;
constexpr FShaderHandle InvalidShaderHandle = InvalidHandle;
constexpr FPSOHandle InvalidPSOHandle = InvalidHandle;
constexpr FVertexFactoryHandle InvalidVertexFactoryHandle = InvalidHandle;

// ---- 着色器阶段 ----
enum class EShaderStage : uint8_t
{
    Vertex,
    Pixel,
    Hull,
    Domain,
    Geometry,       // 预留
    Amplification,  // 预留（第二阶段 Mesh Shading：AS+MS+PS）
    Mesh,           // 预留（第二阶段 Mesh Shading）
    Compute,        // 预留
};

// ---- 渲染通道 ----
// 新增通道（DepthPrepass / 透明 Pass 等）时：追加枚举值即可，处理器与命令桶按下标扩展。
enum class EMeshPass : uint8_t
{
    BasePass = 0,   // 不透明主通道
    Count,
};

// ---- 绘制类型 ----
enum class EDrawType : uint8_t
{
    Indexed,        // DrawIndexedInstanced
    NonIndexed,     // DrawInstanced
    MeshDispatch,   // 预留（第二阶段 AS+MS 管线；FDrawSubmitter 遇到会断言）
};

// ---- 材质管线 ----
enum class EMaterialShaderPipeline : uint8_t
{
    Default,        // VS + PS
    Tessellated,    // VS + (HS + DS) + PS
    MeshShading,    // 预留（第二阶段 AS + MS + PS）
};

// ---- PSO 状态枚举（映射到 d3dx12 预设，见 FGraphicsPSOCache）----
// 顶点布局不在此枚举——由 FVertexFactory 描述（新增布局 = 新工厂，不改枚举）。

enum class EBlendMode : uint8_t
{
    Opaque,
    Masked,         // v1 与 Opaque 同一混合状态（片元裁剪在着色器内完成）
    Translucent,    // 预留
};

enum class ERasterizerMode : uint8_t
{
    SolidCullBack,
    SolidCullNone,
    Wireframe,
};

// View Fill Mode（全局调试开关，属于 View 而非 Material）：Solid 时用材质自身的 Rasterizer State，
// Wireframe 时覆盖为线框。材质注册时为每种模式预建 PSO，切换不在逐帧路径上创建 PSO。
enum class EViewFillMode : uint8_t
{
    Solid,
    Wireframe,
    Count,
};

enum class EDepthMode : uint8_t
{
    ReadWrite,
    ReadOnly,
    Disabled,
};

// ---- 根参数绑定类型 ----
enum class ERootBindingType : uint8_t
{
    CBV,
    Constants,
    DescriptorTable,    // 预留（v1 的 SRV 表由 FDrawSubmitter 统一设置，不进命令）
};

// ---- 编译期常量 ----
inline namespace RendererLimits
{
    constexpr uint32_t MaxVertexStreams = 4;        // 每条命令最多顶点流数
    constexpr uint32_t MaxRootBindings = 4;         // 每条命令最多根参数绑定数
    constexpr uint32_t BindlessSrvCount = 16384;    // bindless CBV/SRV/UAV 堆大小
    constexpr uint32_t FirstTextureBindlessSlot = 1;// 堆槽 0 固定给实例缓冲，纹理从 1 开始
    constexpr uint32_t MaxInstancesPerFrame = 4096; // 每帧槽的实例容量
    constexpr uint32_t NumFrameSlots = 3;           // 与 D3DApp::SwapChainBufferCount 对齐
}
