// FFrameResource.h
// 一个在飞帧（Frame In Flight）的资源：Command Allocator + 该帧提交后的 fence 值。
// 由 D3DApp 按 NumFramesInFlight 轮转持有；复用前框架先 CPU 等待 FenceValue，
// 所以渲染器在 Draw 中按 FrameIndex 重置自己的每帧分区是安全的（应用层不再手写等待）。

#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>

struct FFrameResource
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
    uint64_t FenceValue = 0;    // 该帧最近一次提交的 fence；0 表示从未提交
    uint32_t FrameIndex = 0;    // 帧槽下标（0 .. NumFramesInFlight-1），渲染器的每帧分区用它定位
};
