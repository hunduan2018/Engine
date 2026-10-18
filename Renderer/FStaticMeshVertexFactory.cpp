// FStaticMeshVertexFactory.cpp

#include "FStaticMeshVertexFactory.h"

#include "Mesh/FStaticMesh.h"

namespace
{
    const uint32_t StaticMeshStreamStrides[1] = { FStaticMesh::VertexStrideBytes() };
}

FStaticMeshVertexFactory::FStaticMeshVertexFactory()
    : FVertexFactory(
        {
            // 布局与 FStaticMeshVertex 字段一一对应（44 字节交错单流）。
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  false },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, false },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, false },
            { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, false },
        },
        StaticMeshStreamStrides,
        1,
        "StaticMesh44")
{
}
