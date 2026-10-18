// FMeshRenderData.cpp

#include "FMeshRenderData.h"

#include "../src/Assert.h"
#include "FResourceUploadContext.h"
#include "d3dUtil.h"

FMeshRenderData CreateMeshRenderData(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    const FStaticMesh& staticMesh,
    const FVertexFactory* vertexFactory,
    FResourceUploadContext& uploadContext)
{
    APP_CHECK_MSG(vertexFactory != nullptr, "Mesh render data requires a vertex factory");
    // v1 工厂为单流交错布局；步长与顶点结构必须一致（数据与布局的契约检查）。
    APP_CHECK_MSG(vertexFactory->GetStreamCount() == 1
        && vertexFactory->GetStreamStride(0) == FStaticMesh::VertexStrideBytes(),
        "Vertex factory stride mismatch: FStaticMesh upload requires the StaticMesh44 layout");

    FMeshRenderData renderData;
    renderData.Factory = vertexFactory;
    renderData.BoundsMin = staticMesh.BoundsMin;
    renderData.BoundsMax = staticMesh.BoundsMax;

    const uint32_t vertexStride = vertexFactory->GetStreamStride(0);

    // 顶点缓冲：默认堆 + 临时上传堆（存活期移交 uploadContext）。
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
    renderData.VertexBuffer = d3dUtil::CreateDefaultBuffer(
        device, commandList,
        staticMesh.Vertices.data(),
        static_cast<UINT64>(staticMesh.Vertices.size()) * vertexStride,
        vertexUploadBuffer);
    uploadContext.AddUploadBuffer(std::move(vertexUploadBuffer));

    renderData.VertexBufferView.BufferLocation = renderData.VertexBuffer->GetGPUVirtualAddress();
    renderData.VertexBufferView.SizeInBytes =
        static_cast<UINT>(staticMesh.Vertices.size()) * vertexStride;
    renderData.VertexBufferView.StrideInBytes = vertexStride;

    // 索引缓冲。
    Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;
    renderData.IndexBuffer = d3dUtil::CreateDefaultBuffer(
        device, commandList,
        staticMesh.Indices.data(),
        static_cast<UINT64>(staticMesh.Indices.size()) * sizeof(uint32_t),
        indexUploadBuffer);
    uploadContext.AddUploadBuffer(std::move(indexUploadBuffer));

    renderData.IndexBufferView.BufferLocation = renderData.IndexBuffer->GetGPUVirtualAddress();
    renderData.IndexBufferView.SizeInBytes =
        static_cast<UINT>(staticMesh.Indices.size()) * sizeof(uint32_t);
    renderData.IndexBufferView.Format = FStaticMesh::IndexFormat();

    // section 范围直拷。
    renderData.Sections.reserve(staticMesh.Sections.size());
    for (const FStaticMeshSection& section : staticMesh.Sections)
    {
        FMeshRenderDataSectionRange range = {};
        range.IndexStart = section.IndexStart;
        range.IndexCount = section.IndexCount;
        range.VertexBase = section.VertexBase;
        renderData.Sections.push_back(range);
    }

    return renderData;
}
