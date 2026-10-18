// FScene.cpp

#include "FScene.h"

void FScene::BuildMeshBatches(std::vector<FMeshBatch>& outBatches) const
{
    outBatches.clear();

    for (const FScenePrimitive& primitive : Primitives)
    {
        if (primitive.Mesh == nullptr || primitive.Material == nullptr)
        {
            continue;
        }

        for (uint32_t sectionIndex = 0; sectionIndex < primitive.Mesh->Sections.size(); ++sectionIndex)
        {
            // 线性查找同键批次（场景规模小，避免哈希表开销与复杂度）。
            FMeshBatch* targetBatch = nullptr;
            for (FMeshBatch& batch : outBatches)
            {
                if (batch.Mesh == primitive.Mesh
                    && batch.SectionIndex == sectionIndex
                    && batch.Material == primitive.Material)
                {
                    targetBatch = &batch;
                    break;
                }
            }
            if (targetBatch == nullptr)
            {
                FMeshBatch batch = {};
                batch.Mesh = const_cast<FMeshRenderData*>(primitive.Mesh);
                batch.Material = const_cast<FMaterial*>(primitive.Material);
                batch.SectionIndex = sectionIndex;
                outBatches.push_back(batch);
                targetBatch = &outBatches.back();
            }
            targetBatch->Instances.push_back(primitive.LocalToWorld);
        }
    }
}
