// FDrawSubmitter.h
// 命令执行器：把 FMeshPassDrawList 翻译成 GraphicsCommandList 调用，逐条提交
// （DrawIndexedInstanced / DrawInstanced）。缓存当前管线状态并跳过冗余设置。
// 预留：EDrawType::MeshDispatch 断言拦截（第二阶段 AS+MS）；ExecuteIndirect 变体
// 将消费同样的连续 POD 命令桶（不实现）。

#pragma once

#include "FMeshPassDrawList.h"
#include "FGraphicsPSOCache.h"
#include "FGlobalRootSignature.h"

#include <d3d12.h>

class FDrawSubmitter
{
public:
    // 每帧/命令列表 Reset 后调用，清空状态缓存。
    void ResetCache();

    // 执行一个通道桶：设置描述符堆与根签名（各一次），再按排序顺序逐条提交。
    void ExecuteDraws(const FMeshPassDrawList& drawList,
                      ID3D12GraphicsCommandList* commandList,
                      const FGraphicsPSOCache& psoCache,
                      ID3D12DescriptorHeap* srvHeap);

    // 调试计数：本帧实际发生的 SetPipelineState 次数（验证状态跳过；M3 用）。
    uint32_t GetPipelineStateSetCount() const { return PipelineStateSetCount; }
    uint32_t GetDrawCount() const { return DrawCount; }

private:
    void ApplyCommand(const FMeshDrawCommand& command,
                      ID3D12GraphicsCommandList* commandList,
                      const FGraphicsPSOCache& psoCache);

    // ---- 状态缓存（跳过冗余设置）----
    ID3D12PipelineState* CurrentPipelineState = nullptr;
    ID3D12RootSignature* CurrentRootSignature = nullptr;
    ID3D12DescriptorHeap* CurrentSrvHeap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE CurrentSrvTableGpuHandle = {};
    D3D12_PRIMITIVE_TOPOLOGY CurrentTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    D3D12_VERTEX_BUFFER_VIEW CurrentVertexBuffers[RendererLimits::MaxVertexStreams] = {};
    uint32_t CurrentVertexBufferCount = 0;
    bool bHasIndexBuffer = false;
    D3D12_INDEX_BUFFER_VIEW CurrentIndexBuffer = {};

    // 根绑定缓存：按槽位记录最近一次值。
    bool bSlotHasValue[RootParam::Count] = {};
    D3D12_GPU_VIRTUAL_ADDRESS SlotCbvAddress[RootParam::Count] = {};
    uint32_t SlotConstants[RootParam::Count][4] = {};

    uint32_t PipelineStateSetCount = 0;
    uint32_t DrawCount = 0;
};
