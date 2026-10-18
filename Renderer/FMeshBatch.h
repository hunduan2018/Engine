// FMeshBatch.h
// CPU 侧批处理单元：一个网格 section + 一个材质 + 全部共享该组合的实例变换。
// 合并在 FScene::BuildMeshBatches 中完成——同一 (Mesh, Section, Material) 的实例
// 合成一个批次，最终只产生一条 FMeshDrawCommand（InstanceCount > 1）。

#pragma once

#include "FMaterial.h"
#include "FMeshRenderData.h"
#include "MathTypes.h"

#include <vector>

struct FMeshBatch
{
    FMeshRenderData* Mesh = nullptr;    // 非拥有（自带布局归属 Factory）
    FMaterial* Material = nullptr;      // 非拥有
    uint32_t SectionIndex = 0;

    std::vector<FMatrix4x4> Instances;  // 每实例 LocalToWorld
};
