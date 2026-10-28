// FScene.h
// CPU 侧场景：图元数组（网格 + 材质 + 变换）与视图描述。
// BuildMeshBatches 按 (Mesh, Section, Material) 合并实例，产出 FMeshBatch 列表。

#pragma once

#include "FMaterial.h"
#include "FMeshBatch.h"
#include "MathHelper.h"
#include "MathTypes.h"
#include "RendererTypes.h"

#include <vector>

struct FSceneView
{
    FSimdMatrix View = DirectX::XMMatrixIdentity();
    FSimdMatrix ViewProjection = DirectX::XMMatrixIdentity();
    FVector3 ViewOrigin = { 0.0f, 0.0f, 0.0f };
    FVector3 LightDirection = { 0.0f, -1.0f, 0.0f };    // 指向光源（归一化）
    float TimeSeconds = 0.0f;
    float ViewportWidth = 0.0f;
    float ViewportHeight = 0.0f;
    EViewFillMode FillMode = EViewFillMode::Solid;          // View Mode：渲染时由 Mesh Pass Processor 据此选 PSO
};

struct FScenePrimitive
{
    const FMeshRenderData* Mesh = nullptr;
    const FMaterial* Material = nullptr;
    FMatrix4x4 LocalToWorld = MathHelper::Identity4x4();
};

class FScene
{
public:
    std::vector<FScenePrimitive> Primitives;

    void AddPrimitive(const FMeshRenderData* mesh, const FMaterial* material, const FMatrix4x4& localToWorld)
    {
        FScenePrimitive primitive = {};
        primitive.Mesh = mesh;
        primitive.Material = material;
        primitive.LocalToWorld = localToWorld;
        Primitives.push_back(primitive);
    }

    // 合并规则：同一 (Mesh, Section, Material) 的所有实例进入同一批次。
    // v1 假定一个图元整体使用一个材质（各 section 的 MaterialIndex 暂不消费）。
    void BuildMeshBatches(std::vector<FMeshBatch>& outBatches) const;
};
