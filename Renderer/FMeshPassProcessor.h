// FMeshPassProcessor.h
// 渲染通道处理器抽象：把 FMeshBatch（网格 + 材质 + 实例）翻译成目标通道的
// FMeshDrawCommand。新增通道（DepthPrepass / 透明 Pass）= 新增一个子类 + 一个桶，
// 渲染器主流程零改动（开闭原则）。

#pragma once

#include "FMeshBatch.h"
#include "FMeshDrawCommand.h"
#include "FMaterialConstantCache.h"
#include "FMeshPassDrawList.h"
#include "FScene.h"
#include "FUploadConstantRing.h"

#include <d3d12.h>

class FInstanceAllocator;

// 处理器构建命令所需的一切（单向依赖：处理器只依赖此上下文，不依赖 FRenderer 具体类型）。
struct FMeshPassProcessorContext
{
    FUploadConstantRing* ConstantRing = nullptr;
    FMaterialConstantCache* MaterialConstants = nullptr;   // 材质 CB 每帧缓存（b1 地址）
    FInstanceAllocator* Instances = nullptr;
    ID3D12RootSignature* RootSignature = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS ViewCbGpuAddress = 0;    // 本帧视图常量 CBV 地址（b0）
    uint32_t FrameSlot = 0;
    FSceneView View;
};

class FMeshPassProcessor
{
public:
    virtual ~FMeshPassProcessor() = default;

    // 为一个批次构建一条或多条命令，写入构造时绑定的通道桶。
    virtual void AddMeshBatch(const FMeshBatch& batch) = 0;

protected:
    FMeshPassProcessor(FMeshPassProcessorContext& inContext, FMeshPassDrawList& inDrawList, EMeshPass inPassId);

    // 共享命令组装：解析材质 PSO、写材质常量、拷实例矩阵、填充管线/绑定/流/绘制参数/排序键。
    void BuildMeshDrawCommand(const FMeshBatch& batch);

    FMeshPassProcessorContext& Context;
    FMeshPassDrawList& DrawList;
    EMeshPass PassId;
    uint16_t Sequence = 0;
};
