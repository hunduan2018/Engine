// FMeshRenderData.h
// 网格渲染资源（UE5 FStaticMeshRenderData 的单 LOD 裁剪）：GPU 侧 VB/IB 与视图、
// section 范围、布局归属（VertexFactory）与包围盒。
// 布局由工厂描述——本类型不感知具体 IA 布局，VBV 步长从工厂读取。
// 上传仅在初始化期发生一次（经 FResourceUploadContext 存活到 GPU 完成）。
// 预留：第二阶段 Mesh Shading 的 meshlet 缓冲在工厂扩展流 SRV 后追加于此。

#pragma once

#include "FVertexFactory.h"
#include "MathTypes.h"
#include "Mesh/FStaticMesh.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <vector>

class FResourceUploadContext;

struct FMeshRenderDataSectionRange
{
    uint32_t IndexStart = 0;
    uint32_t IndexCount = 0;
    int32_t VertexBase = 0;
};

struct FMeshRenderData
{
    // d3dUtil::CreateDefaultBuffer 结束于 GENERIC_READ（已含 VB/IB 读取位），
    // 初始化期 fence 等待后即可直接供 IA 使用，无需额外屏障。
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

    std::vector<FMeshRenderDataSectionRange> Sections;

    // 布局归属（决定 PSO 输入布局与 VBV 步长；非拥有，指向工厂注册表实例）。
    const FVertexFactory* Factory = nullptr;

    // 包围盒（自 FStaticMesh 迁移；供将来视锥剔除）。
    FVector3 BoundsMin = { 0.0f, 0.0f, 0.0f };
    FVector3 BoundsMax = { 0.0f, 0.0f, 0.0f };
};

// 把 CPU 侧 FStaticMesh 上传为 GPU 渲染资源；vertexFactory 决定布局（步长须与顶点尺寸一致）。
// commandList 须处于录制状态。
FMeshRenderData CreateMeshRenderData(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    const FStaticMesh& staticMesh,
    const FVertexFactory* vertexFactory,
    FResourceUploadContext& uploadContext);
