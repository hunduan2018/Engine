// FUploadConstantRing.h
// 每帧槽一块的常量上传环：持久映射的 UPLOAD 缓冲，BeginFrame 重置该槽偏移。
// 生命周期约定：调用方（TestApp::Draw）必须先 CPU 等待该帧槽的 fence，再 BeginFrame。

#pragma once

#include "RendererTypes.h"

#include <d3d12.h>
#include <wrl.h>

struct FRingAllocation
{
    uint8_t* CpuAddress = nullptr;              // 映射地址（可直接写入）
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;   // 用于根 CBV 绑定
};

class FUploadConstantRing
{
public:
    FUploadConstantRing() = default;
    ~FUploadConstantRing();

    FUploadConstantRing(const FUploadConstantRing&) = delete;
    FUploadConstantRing& operator=(const FUploadConstantRing&) = delete;

    // slotSizeBytes：每帧槽容量（建议 1MB）。
    void Initialize(ID3D12Device* device, uint32_t numSlots, uint64_t slotSizeBytes);

    // 每帧调用一次：重置该槽线性偏移。断言槽号按 0,1,2,0.. 轮转，防止乱序复用。
    void BeginFrame(uint32_t frameSlot);

    // 分配并返回可写内存；对齐必须是 256 的倍数（CBV 要求）。
    FRingAllocation Alloc(uint32_t frameSlot, uint64_t sizeBytes, uint64_t alignment = 256);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffers[RendererLimits::NumFrameSlots];
    uint8_t* MappedAddresses[RendererLimits::NumFrameSlots] = {};
    D3D12_GPU_VIRTUAL_ADDRESS GpuBaseAddresses[RendererLimits::NumFrameSlots] = {};
    uint64_t SlotSizes[RendererLimits::NumFrameSlots] = {};
    uint64_t SlotOffsets[RendererLimits::NumFrameSlots] = {};
    uint32_t SlotCount = 0;
    // 初始值 = 最后一个槽：首次 BeginFrame(0) 的期望槽 (LastResetSlot+1)%SlotCount == 0。
    uint32_t LastResetSlot = RendererLimits::NumFrameSlots - 1;
};
