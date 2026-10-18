// FInstanceAllocator.cpp

#include "FInstanceAllocator.h"

#include "../src/Assert.h"
#include "d3dUtil.h"
#include "d3dx12.h"

FInstanceAllocator::~FInstanceAllocator()
{
    if (UploadStaging && MappedStaging)
    {
        UploadStaging->Unmap(0, nullptr);
        MappedStaging = nullptr;
    }
}

void FInstanceAllocator::Initialize(ID3D12Device* device)
{
    // 总容量 = 帧槽数 x 每帧实例上限；两个缓冲同布局，逐槽对应拷贝。
    const uint64_t totalBytes = static_cast<uint64_t>(RendererLimits::NumFrameSlots)
        * RendererLimits::MaxInstancesPerFrame * sizeof(FInstanceData);
    const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE,
        &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&InstanceBuffer)));
    InstanceBuffer->SetName(L"FInstanceAllocator DefaultBuffer");

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&UploadStaging)));
    UploadStaging->SetName(L"FInstanceAllocator Staging");

    D3D12_THROW_IF_FAILED(UploadStaging->Map(0, nullptr,
        reinterpret_cast<void**>(&MappedStaging)));
}

void FInstanceAllocator::CreateInstanceBufferSRV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
{
    APP_CHECK(InstanceBuffer != nullptr);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = RendererLimits::NumFrameSlots * RendererLimits::MaxInstancesPerFrame;
    srvDesc.Buffer.StructureByteStride = sizeof(FInstanceData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(InstanceBuffer.Get(), &srvDesc, cpuHandle);
}

void FInstanceAllocator::BeginFrame(uint32_t frameSlot)
{
    APP_CHECK(frameSlot < RendererLimits::NumFrameSlots);
    SlotOffsets[frameSlot] = 0;
    bSlotFlushed[frameSlot] = false;
    CurrentSlot = frameSlot;
}

uint32_t FInstanceAllocator::Alloc(uint32_t frameSlot, const std::vector<FMatrix4x4>& instances)
{
    APP_CHECK(frameSlot == CurrentSlot);
    APP_CHECK_MSG(!bSlotFlushed[frameSlot],
        "Alloc after FlushInstanceCopies for this frame slot would miss the copy");
    APP_CHECK_MSG(!instances.empty(), "FMeshBatch must carry at least one instance");
    APP_CHECK_MSG(SlotOffsets[frameSlot] + instances.size() <= RendererLimits::MaxInstancesPerFrame,
        "Instance budget for this frame slot exhausted; raise MaxInstancesPerFrame");

    // 分区起点 + 当前偏移 = 实例缓冲中的绝对下标（着色器按此 + SV_InstanceID 寻址）。
    const uint32_t slotBase = frameSlot * RendererLimits::MaxInstancesPerFrame;
    const uint32_t location = slotBase + SlotOffsets[frameSlot];

    // 先写暂存（转置：着色器 mul(向量, 矩阵) 行向量约定），Flush 时拷入默认堆。
    FInstanceData* destination = MappedStaging + location;
    for (size_t i = 0; i < instances.size(); ++i)
    {
        using namespace DirectX;
        const FSimdMatrix matrix = XMMatrixTranspose(XMLoadFloat4x4(&instances[i]));
        XMStoreFloat4x4(&destination[i].LocalToWorld, matrix);
    }

    SlotOffsets[frameSlot] += static_cast<uint32_t>(instances.size());
    return location;
}

void FInstanceAllocator::FlushInstanceCopies(ID3D12GraphicsCommandList* commandList)
{
    const uint32_t frameSlot = CurrentSlot;
    APP_CHECK(frameSlot < RendererLimits::NumFrameSlots);
    if (bSlotFlushed[frameSlot] || SlotOffsets[frameSlot] == 0)
    {
        return;
    }

    const UINT64 slotBytes = static_cast<UINT64>(SlotOffsets[frameSlot]) * sizeof(FInstanceData);
    const UINT64 slotOffsetBytes = static_cast<UINT64>(frameSlot) * RendererLimits::MaxInstancesPerFrame
        * sizeof(FInstanceData);

    // 一对屏障 + 区域拷贝（只拷本帧实际写入量）。
    CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        InstanceBuffer.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &toCopyDest);

    commandList->CopyBufferRegion(
        InstanceBuffer.Get(), slotOffsetBytes,
        UploadStaging.Get(), slotOffsetBytes,
        slotBytes);

    CD3DX12_RESOURCE_BARRIER toGenericRead = CD3DX12_RESOURCE_BARRIER::Transition(
        InstanceBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    commandList->ResourceBarrier(1, &toGenericRead);

    bSlotFlushed[frameSlot] = true;
}
