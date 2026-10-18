// FResourceUploadContext.h
// 初始化期上传资源存活助手：CreateDefaultBuffer / 纹理上传产生的临时 upload 堆
// 必须存活到 GPU 完成拷贝；本对象持有它们直到 FlushAndWait。

#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <vector>

class Direct3DQueue;

class FResourceUploadContext
{
public:
    // 登记一个临时上传缓冲（调用后所有权转移，GPU 完成前不得释放）。
    void AddUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer);

    // 提交命令列表（队列负责 Close）、Signal 并 CPU 阻塞等待，然后释放全部临时资源。
    // commandList 处于录制状态传入；返回后处于关闭状态（供后续 Reset）。
    void FlushAndWait(Direct3DQueue* graphicsQueue, ID3D12GraphicsCommandList* commandList);

private:
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> UploadBuffers;
};
