// FRenderer.h
// 渲染器聚合入口：拥有全部渲染子系统（着色器字典、PSO 缓存、全局根签名、
// bindless 堆、常量上传环、实例缓冲、各通道命令桶、命令执行器），
// 对外暴露 初始化 / 材质注册 / 每帧 BeginFrame-UpdateView-RenderScene 流程。

#pragma once

#include "RendererTypes.h"
#include "FShaderManager.h"
#include "FGraphicsPSOCache.h"
#include "FGlobalRootSignature.h"
#include "FUploadConstantRing.h"
#include "FResourceUploadContext.h"
#include "FInstanceAllocator.h"
#include "FMaterialConstantCache.h"
#include "FMeshPassDrawList.h"
#include "FDrawSubmitter.h"
#include "FVertexFactory.h"
#include "FScene.h"

#include "DescriptorHeapManagement.h"

#include <d3d12.h>

#include <memory>

class Direct3DQueue;

// b0 视图常量的 CPU/GPU 镜像；字段须与 shader_common.hlsli 的 FViewConstants 一致。
struct FViewConstants
{
    FMatrix4x4 View;            // 已转置（mul(向量, 矩阵) 约定）
    FMatrix4x4 ViewProjection;  // 已转置
    FVector4 ViewOriginTime;    // xyz = 视点位置, w = 时间秒
    FVector4 LightDirection;    // xyz = 指向光源, w 未用
    FVector4 Resolution;        // xy = 视口尺寸, zw 未用
};

class FRenderer
{
public:
    // subsystem 初始化 + 命令列表进入录制状态（供随后的资产上传录制）。
    // initAllocator 只用于初始化期（调用方保证其后 GPU 已空闲才可复用）。
    void Initialize(ID3D12Device* device,
                    ID3D12CommandAllocator* initAllocator,
                    ID3D12GraphicsCommandList* commandList,
                    Direct3DQueue* graphicsQueue,
                    DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

    // 执行初始化期命令列表并 CPU 等待；此后临时上传堆被释放，默认堆资源可用。
    void FlushInitUploads();

    // ---- 初始化期资产创建入口（TestApp 使用）----
    FShaderManager& GetShaderManager() { return ShaderManager; }
    BindlessAllocator& GetBindlessAllocator() { return *Bindless; }
    FResourceUploadContext& GetUploadContext() { return UploadContext; }

    // 注册顶点工厂（分配布局句柄；Initialize 已注册默认静态网格工厂）。
    const FVertexFactory* RegisterVertexFactory(std::unique_ptr<FVertexFactory> factory);
    const FVertexFactory* GetStaticMeshVertexFactory() const { return StaticMeshVertexFactory; }

    // 注册材质：布局来自 vertexFactory、拓扑来自材质管线，二者解耦；
    // 为每个通道预创建 PSO（唯一的 PSO 创建入口）并分配排序 ID。
    // v1 材质×工厂一对一；多工厂共享材质时扩展 PipelineStates 维度（预留）。
    void RegisterMaterial(FMaterial* material, const FVertexFactory* vertexFactory);

    // ---- 每帧流程 ----
    // 须在该帧槽 fence CPU 等待之后调用。
    void BeginFrame(uint32_t frameSlot);
    // 写入本帧视图常量（b0 内容）。
    void UpdateView(const FSceneView& view);
    // 批次构建 -> 通道处理器 -> 排序 -> 逐条提交。
    void RenderScene(const FScene& scene, ID3D12GraphicsCommandList* commandList);

    // 调试：各通道命令数与提交统计（M2/M3 验证用）。
    uint32_t GetPassCommandCount(EMeshPass pass) const { return PassDrawLists[static_cast<size_t>(pass)].Num(); }
    uint32_t GetLastPipelineStateSetCount() const { return DrawSubmitter.GetPipelineStateSetCount(); }
    uint32_t GetLastDrawCount() const { return DrawSubmitter.GetDrawCount(); }

private:
    ID3D12Device* Device = nullptr;
    ID3D12GraphicsCommandList* InitCommandList = nullptr;
    Direct3DQueue* GraphicsQueue = nullptr;
    DXGI_FORMAT RtvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT DsvFormat = DXGI_FORMAT_UNKNOWN;

    FShaderManager ShaderManager;
    FGraphicsPSOCache PsoCache;
    FGlobalRootSignature GlobalRootSignature;
    std::vector<std::unique_ptr<FVertexFactory>> VertexFactories;   // 工厂注册表（句柄=下标）
    const FVertexFactory* StaticMeshVertexFactory = nullptr;        // 默认布局缓存
    std::unique_ptr<BindlessAllocator> Bindless;
    FUploadConstantRing ConstantRing;
    FInstanceAllocator InstanceAllocator;
    FMaterialConstantCache MaterialConstants;
    FResourceUploadContext UploadContext;
    FMeshPassDrawList PassDrawLists[static_cast<size_t>(EMeshPass::Count)];
    FDrawSubmitter DrawSubmitter;

    uint32_t CurrentFrameSlot = 0;
    D3D12_GPU_VIRTUAL_ADDRESS ViewCbGpuAddress = 0;
    FSceneView CurrentView;
    uint16_t NextMaterialSortId = 0;

    std::vector<FMeshBatch> TmpBatches;   // 每帧复用，避免反复分配
};
