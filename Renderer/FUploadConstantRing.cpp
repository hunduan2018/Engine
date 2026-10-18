// FUploadConstantRing.cpp

#include "FUploadConstantRing.h"

#include "../src/Assert.h"
#include "d3dUtil.h"

FUploadConstantRing::~FUploadConstantRing()
{
    for (uint32_t slot = 0; slot < SlotCount; ++slot)
    {
        if (UploadBuffers[slot] && MappedAddresses[slot])
        {
            UploadBuffers[slot]->Unmap(0, nullptr);
            MappedAddresses[slot] = nullptr;
        }
    }
}

void FUploadConstantRing::Initialize(ID3D12Device* device, uint32_t numSlots, uint64_t slotSizeBytes)
{
    APP_CHECK(numSlots <= RendererLimits::NumFrameSlots);

    for (uint32_t slot = 0; slot < numSlots; ++slot)
    {
        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(slotSizeBytes);
        D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE,
            &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&UploadBuffers[slot])));

        D3D12_THROW_IF_FAILED(UploadBuffers[slot]->Map(0, nullptr,
            reinterpret_cast<void**>(&MappedAddresses[slot])));
        GpuBaseAddresses[slot] = UploadBuffers[slot]->GetGPUVirtualAddress();
        SlotSizes[slot] = slotSizeBytes;
        SlotOffsets[slot] = 0;
    }
    SlotCount = numSlots;
}

void FUploadConstantRing::BeginFrame(uint32_t frameSlot)
{
    // 槽按轮转顺序复用：帧 N 与帧 N+3 使用同一块内存，只有按序推进才能保证
    // 重置时 GPU 已越过该槽上一次的提交（配合调用方的 fence CPU 等待）。
    // 窗口 resize 会把后备缓冲索引重置为 0，打破轮转；OnResize 前调用方已
    // flush 全部队列，跳变时直接接受并重新同步轮转即可。
    const uint32_t expectedSlot = (LastResetSlot + 1) % SlotCount;
    if (frameSlot != expectedSlot)
    {
        LastResetSlot = (frameSlot + SlotCount - 1) % SlotCount;
    }
    SlotOffsets[frameSlot] = 0;
    LastResetSlot = frameSlot;
}

FRingAllocation FUploadConstantRing::Alloc(uint32_t frameSlot, uint64_t sizeBytes, uint64_t alignment)
{
    APP_CHECK(frameSlot < SlotCount);
    APP_CHECK(alignment > 0 && (alignment & (alignment - 1)) == 0);

    const uint64_t alignedOffset = (SlotOffsets[frameSlot] + alignment - 1) & ~(alignment - 1);
    APP_CHECK_MSG(alignedOffset + sizeBytes <= SlotSizes[frameSlot],
        "FUploadConstantRing slot exhausted; increase slot size");

    SlotOffsets[frameSlot] = alignedOffset + sizeBytes;
    FRingAllocation allocation = {};
    allocation.CpuAddress = MappedAddresses[frameSlot] + alignedOffset;
    allocation.GpuAddress = GpuBaseAddresses[frameSlot] + alignedOffset;
    return allocation;
}
