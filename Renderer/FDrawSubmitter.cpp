// FDrawSubmitter.cpp

#include "FDrawSubmitter.h"

#include "../src/Assert.h"
#include "FGlobalRootSignature.h"

#include <cstring>

void FDrawSubmitter::ResetCache()
{
    CurrentPipelineState = nullptr;
    CurrentRootSignature = nullptr;
    CurrentSrvHeap = nullptr;
    CurrentSrvTableGpuHandle = {};
    CurrentTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    CurrentVertexBufferCount = 0;
    bHasIndexBuffer = false;
    memset(CurrentVertexBuffers, 0, sizeof(CurrentVertexBuffers));
    memset(&CurrentIndexBuffer, 0, sizeof(CurrentIndexBuffer));
    memset(bSlotHasValue, 0, sizeof(bSlotHasValue));
    memset(SlotCbvAddress, 0, sizeof(SlotCbvAddress));
    memset(SlotConstants, 0, sizeof(SlotConstants));
    PipelineStateSetCount = 0;
    DrawCount = 0;
}

void FDrawSubmitter::ExecuteDraws(const FMeshPassDrawList& drawList,
                                  ID3D12GraphicsCommandList* commandList,
                                  const FGraphicsPSOCache& psoCache,
                                  ID3D12DescriptorHeap* srvHeap)
{
    // bindless SRV 堆：每帧只设一次，并在根签名就绪后绑定 descriptor table
    //（表的 GPU 起始 = 堆起始，覆盖 t0.. 全部 SRV；缺失此绑定会导致表内资源全读零）。
    if (CurrentSrvHeap != srvHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        CurrentSrvHeap = srvHeap;
        CurrentSrvTableGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
        // 根签名未设置时表在首命令 RS 设置后补绑（见 ApplyCommand）。
        if (CurrentRootSignature != nullptr)
        {
            commandList->SetGraphicsRootDescriptorTable(RootParam::BindlessSrvTable, CurrentSrvTableGpuHandle);
        }
    }

    for (uint32_t index = 0; index < drawList.Num(); ++index)
    {
        ApplyCommand(drawList.GetCommand(index), commandList, psoCache);
    }
}

void FDrawSubmitter::ApplyCommand(const FMeshDrawCommand& command,
                                  ID3D12GraphicsCommandList* commandList,
                                  const FGraphicsPSOCache& psoCache)
{
    // 第二阶段预留：MeshDispatch 需走 DispatchMesh 路径，当前断言拦截。
    APP_CHECK_MSG(command.DrawType == EDrawType::Indexed || command.DrawType == EDrawType::NonIndexed,
        "EDrawType::MeshDispatch is reserved for the mesh shading stage");

    // 根签名（v1 全局唯一，首条命令设置一次）。
    if (CurrentRootSignature != command.RootSignature)
    {
        commandList->SetGraphicsRootSignature(command.RootSignature);
        CurrentRootSignature = command.RootSignature;
        // 根签名更换后所有根参数失效，需重绑 bindless 表（堆句柄经 ExecuteDraws 缓存）。
        memset(bSlotHasValue, 0, sizeof(bSlotHasValue));
        if (CurrentSrvHeap != nullptr)
        {
            commandList->SetGraphicsRootDescriptorTable(RootParam::BindlessSrvTable, CurrentSrvTableGpuHandle);
        }
    }

    // PSO。
    ID3D12PipelineState* pipelineState = psoCache.GetPipelineState(command.PipelineState);
    if (CurrentPipelineState != pipelineState)
    {
        commandList->SetPipelineState(pipelineState);
        CurrentPipelineState = pipelineState;
        ++PipelineStateSetCount;
    }

    // 根绑定：按槽位对比，仅变更时下发。
    for (uint32_t i = 0; i < command.RootBindingCount; ++i)
    {
        const FMeshDrawCommand::FRootBinding& binding = command.RootBindings[i];
        APP_CHECK(binding.SlotIndex < RootParam::Count);

        if (binding.Type == ERootBindingType::CBV)
        {
            if (!bSlotHasValue[binding.SlotIndex] || SlotCbvAddress[binding.SlotIndex] != binding.GpuAddress)
            {
                commandList->SetGraphicsRootConstantBufferView(binding.SlotIndex, binding.GpuAddress);
                SlotCbvAddress[binding.SlotIndex] = binding.GpuAddress;
                bSlotHasValue[binding.SlotIndex] = true;
            }
        }
        else if (binding.Type == ERootBindingType::Constants)
        {
            if (!bSlotHasValue[binding.SlotIndex]
                || memcmp(SlotConstants[binding.SlotIndex], binding.Constants, sizeof(binding.Constants)) != 0)
            {
                commandList->SetGraphicsRoot32BitConstants(
                    binding.SlotIndex, binding.NumConstants, binding.Constants, 0);
                memcpy(SlotConstants[binding.SlotIndex], binding.Constants, sizeof(binding.Constants));
                bSlotHasValue[binding.SlotIndex] = true;
            }
        }
        else
        {
            // DescriptorTable 绑定类型预留（v1 的 SRV 表由桶级一次性设置）。
            APP_CHECK_MSG(false, "ERootBindingType::DescriptorTable is reserved");
        }
    }

    // 顶点流：流数变化整组重设，否则只重设有差异的流。
    if (CurrentVertexBufferCount != command.VertexStreamCount)
    {
        commandList->IASetVertexBuffers(0, command.VertexStreamCount, command.VertexBufferViews);
        memcpy(CurrentVertexBuffers, command.VertexBufferViews, sizeof(CurrentVertexBuffers));
        CurrentVertexBufferCount = command.VertexStreamCount;
    }
    else
    {
        for (uint32_t stream = 0; stream < command.VertexStreamCount; ++stream)
        {
            if (memcmp(&CurrentVertexBuffers[stream], &command.VertexBufferViews[stream],
                       sizeof(D3D12_VERTEX_BUFFER_VIEW)) != 0)
            {
                commandList->IASetVertexBuffers(stream, 1, &command.VertexBufferViews[stream]);
                CurrentVertexBuffers[stream] = command.VertexBufferViews[stream];
            }
        }
    }

    // 索引缓冲（仅 Indexed 命令需要）。
    if (command.DrawType == EDrawType::Indexed)
    {
        if (!bHasIndexBuffer || memcmp(&CurrentIndexBuffer, &command.IndexBufferView,
                                       sizeof(D3D12_INDEX_BUFFER_VIEW)) != 0)
        {
            commandList->IASetIndexBuffer(&command.IndexBufferView);
            CurrentIndexBuffer = command.IndexBufferView;
            bHasIndexBuffer = true;
        }
    }

    // 拓扑。
    if (CurrentTopology != command.PrimitiveTopology)
    {
        commandList->IASetPrimitiveTopology(command.PrimitiveTopology);
        CurrentTopology = command.PrimitiveTopology;
    }

    // 提交绘制。
    if (command.DrawType == EDrawType::Indexed)
    {
        commandList->DrawIndexedInstanced(
            command.IndexCountPerInstance, command.InstanceCount,
            command.StartIndexLocation, command.BaseVertexLocation,
            command.StartInstanceLocation);
    }
    else
    {
        commandList->DrawInstanced(
            command.IndexCountPerInstance, command.InstanceCount,
            command.StartIndexLocation, command.StartInstanceLocation);
    }
    ++DrawCount;
}
