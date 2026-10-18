// FBasePassProcessor.h
// 不透明 BasePass 处理器：深度读写、无混合。v1 每批次产一条命令（实例合并在
// FScene::BuildMeshBatches 已完成）。后续通道按此模式新增子类即可。

#pragma once

#include "FMeshPassProcessor.h"

class FBasePassProcessor : public FMeshPassProcessor
{
public:
    FBasePassProcessor(FMeshPassProcessorContext& context, FMeshPassDrawList& drawList)
        : FMeshPassProcessor(context, drawList, EMeshPass::BasePass)
    {
    }

    void AddMeshBatch(const FMeshBatch& batch) override
    {
        BuildMeshDrawCommand(batch);
    }
};
