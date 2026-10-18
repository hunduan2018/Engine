// FInstanceAllocator.h
// 实例数据分配器：默认堆缓冲承载全部实例矩阵（SRV 固定占 bindless 堆槽 0 = t0），
// CPU 每帧先写上传暂存，RenderScene 提交绘制前经 CopyBufferRegion 拷入默认堆。
// 布局：单缓冲按帧槽分区（3 x MaxInstancesPerFrame），命令只需 StartInstanceLocation
// 根常量——实例合并免费，未来 ExecuteIndirect 也无需额外布局。
// 为什么默认堆而非直接映射上传堆：实例矩阵被 VS 每顶点 / DS 每细分点随机读取，
// 上传堆（系统内存）意味着每次读取跨 PCIe；UE5 GPUScene 同样把实例数据放默认堆、
// 经 staging 上传（GPUScene.cpp:1091, D3D12Buffer.cpp:444）。

#pragma once

#include "RendererTypes.h"
#include "MathTypes.h"

#include <d3d12.h>
#include <wrl.h>

#include <vector>

// GPU 镜像；字段须与 shader_common.hlsli 的 FInstanceData 一致（64 字节）。
struct FInstanceData
{
    FMatrix4x4 LocalToWorld;    // 写入时已转置（着色器 mul(向量, 矩阵) 约定）
};

class FInstanceAllocator
{
public:
    FInstanceAllocator() = default;
    ~FInstanceAllocator();

    FInstanceAllocator(const FInstanceAllocator&) = delete;
    FInstanceAllocator& operator=(const FInstanceAllocator&) = delete;

    // 创建默认堆实例缓冲 + 上传暂存缓冲（同为 3 帧槽分区、持久映射暂存）。
    void Initialize(ID3D12Device* device);

    // 把 SRV 写入 bindless 堆槽 0（FRenderer 必须先 Allocate 出槽 0 并传入句柄）。
    void CreateInstanceBufferSRV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    // 每帧调用：重置该帧槽的分配偏移（须在该槽 fence 等待之后）。
    void BeginFrame(uint32_t frameSlot);

    // 写入 count 个实例矩阵（经暂存缓冲）并返回实例缓冲中的绝对起始下标。
    uint32_t Alloc(uint32_t frameSlot, const std::vector<FMatrix4x4>& instances);

    // 把本帧槽已写入的暂存数据拷入默认堆（一对屏障 + CopyBufferRegion）。
    // 每帧槽只执行一次；必须在读取该数据的绘制提交之前调用。
    void FlushInstanceCopies(ID3D12GraphicsCommandList* commandList);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> InstanceBuffer;      // 默认堆，GENERIC_READ
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadStaging;       // 上传堆，持久映射
    FInstanceData* MappedStaging = nullptr;
    uint32_t SlotOffsets[RendererLimits::NumFrameSlots] = {};
    bool bSlotFlushed[RendererLimits::NumFrameSlots] = {};
    uint32_t CurrentSlot = 0;
};
