// FResourceUploadContext.cpp

#include "FResourceUploadContext.h"

#include "D3D12QueueManger.h"

void FResourceUploadContext::AddUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer)
{
    UploadBuffers.push_back(std::move(uploadBuffer));
}

void FResourceUploadContext::FlushAndWait(Direct3DQueue* graphicsQueue, ID3D12GraphicsCommandList* commandList)
{
    // 队列实现负责 Close 列表、执行、Signal 并返回 fence 值。
    ID3D12CommandList* commandLists[] = { commandList };
    const uint64_t fenceValue = graphicsQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // CPU 阻塞等待：默认堆资源此后可安全使用，临时 upload 堆可释放。
    graphicsQueue->WaitForFenceCPUBlocking(fenceValue);

    UploadBuffers.clear();
}
