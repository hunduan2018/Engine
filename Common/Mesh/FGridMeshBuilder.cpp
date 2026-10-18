// FGridMeshBuilder.cpp

#include "FGridMeshBuilder.h"

FStaticMesh FGridMeshBuilder::BuildTessellatedPlane(float size, uint32_t divisions)
{
    FStaticMesh mesh;

    const float cellSize = size / static_cast<float>(divisions);
    const float halfSize = size * 0.5f;

    // 纹理按世界单位平铺（每 20 单位重复一次）。
    const float uvScale = 1.0f / 20.0f;

    mesh.Vertices.reserve(static_cast<size_t>(divisions) * divisions * 6);
    mesh.Indices.reserve(static_cast<size_t>(divisions) * divisions * 6);

    const auto appendVertex = [&mesh, uvScale](float x, float z) -> uint32_t
    {
        FStaticMeshVertex vertex = {};
        vertex.Position = FVector3(x, 0.0f, z);
        vertex.Normal = FVector3(0.0f, 1.0f, 0.0f);
        vertex.UV0 = FVector2(x * uvScale, z * uvScale);
        vertex.Tangent = FVector3(1.0f, 0.0f, 0.0f);
        const uint32_t index = static_cast<uint32_t>(mesh.Vertices.size());
        mesh.Vertices.push_back(vertex);
        return index;
    };

    for (uint32_t row = 0; row < divisions; ++row)
    {
        for (uint32_t column = 0; column < divisions; ++column)
        {
            const float x0 = -halfSize + column * cellSize;
            const float z0 = -halfSize + row * cellSize;
            const float x1 = x0 + cellSize;
            const float z1 = z0 + cellSize;

            // 两个三角形，各自独立 3 顶点：顶点不复用（patch = 三角形）。
            // 绕序（从 +Y 俯视，CullNone 材质下不敏感，仍按逆时针保持一致）。
            mesh.Indices.push_back(appendVertex(x0, z0));
            mesh.Indices.push_back(appendVertex(x1, z0));
            mesh.Indices.push_back(appendVertex(x0, z1));

            mesh.Indices.push_back(appendVertex(x1, z0));
            mesh.Indices.push_back(appendVertex(x1, z1));
            mesh.Indices.push_back(appendVertex(x0, z1));
        }
    }

    FStaticMeshSection section = {};
    section.Name = "GroundPlane";
    section.MaterialIndex = 0;
    section.IndexStart = 0;
    section.IndexCount = static_cast<uint32_t>(mesh.Indices.size());
    section.VertexBase = 0;
    mesh.Sections.push_back(section);

    mesh.MaterialNames.push_back("Ground");
    mesh.RecomputeBounds();
    return mesh;
}
