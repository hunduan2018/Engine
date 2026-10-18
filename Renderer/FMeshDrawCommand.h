// FMeshDrawCommand.h
// 渲染命令核心结构：包含一次绘制所需的全部 GPU 管线信息（PSO、根绑定、顶点流、
// 索引、拓扑、绘制参数、排序键）。连续 POD、无堆数据——未来 ExecuteIndirect 路径
// 只需把命令镜像到 GPU 可见缓冲（设计预留，本阶段不实现）。

#pragma once

#include "RendererTypes.h"

#include <d3d12.h>

#include <cstdint>

class FMeshDrawCommand
{
public:
    struct FRootBinding
    {
        ERootBindingType Type = ERootBindingType::CBV;
        uint32_t SlotIndex = 0;                                 // 根参数索引（对应 FGlobalRootSignature）
        uint32_t NumConstants = 0;                              // 仅 Constants 类型使用
        D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;               // 仅 CBV 类型使用
        uint32_t Constants[4] = {};                             // 仅 Constants 类型使用（原始 UINT）
    };

    // ---- 管线 ----
    FPSOHandle PipelineState = InvalidPSOHandle;
    ID3D12RootSignature* RootSignature = nullptr;               // v1：全局根签名

    // ---- 根绑定（按 SlotIndex 升序填充）----
    FRootBinding RootBindings[RendererLimits::MaxRootBindings] = {};
    uint32_t RootBindingCount = 0;

    // ---- 几何流 ----
    D3D12_VERTEX_BUFFER_VIEW VertexBufferViews[RendererLimits::MaxVertexStreams] = {};
    uint32_t VertexStreamCount = 0;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    EDrawType DrawType = EDrawType::Indexed;

    // ---- 绘制参数（DrawIndexedInstanced / DrawInstanced；MeshDispatch 预留）----
    uint32_t IndexCountPerInstance = 0;
    uint32_t InstanceCount = 0;
    uint32_t StartIndexLocation = 0;
    int32_t BaseVertexLocation = 0;
    uint32_t StartInstanceLocation = 0;                         // 实例缓冲（g_InstanceData）中的起始下标

    // ---- 排序 ----
    uint64_t SortKey = 0;
};

// 不透明排序键：状态变更最小化顺序。
// [63..48] PassId（多 Pass 路由预留）| [47..32] PSO | [31..16] 材质 | [15..0] 序列号。
// 未来扩展：DepthPrepass 低 16 位换成前向深度；透明 Pass 整体逆序。
uint64_t ComposeOpaqueSortKey(uint16_t passId, uint16_t psoIndex, uint16_t materialId, uint16_t sequence);
