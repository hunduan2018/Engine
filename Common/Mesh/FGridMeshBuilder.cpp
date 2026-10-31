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

FStaticMesh FBoxMeshBuilder::BuildBox(float size)
{
    FStaticMesh mesh;
    const float halfSize = size * 0.5f;
    // 纹理按世界单位平铺（每 20 单位重复一次）。
    const float uvScale = 1.0f / 20.0f;
    mesh.Vertices.reserve(24);
    mesh.Indices.reserve(36);
    const auto appendVertex = [&mesh, uvScale,size](float x, float y, float z, float nx, float ny, float nz, float u, float v) -> uint32_t
    {
        FStaticMeshVertex vertex = {};
        vertex.Position = FVector3(x, y, z);
        vertex.Normal = FVector3(nx, ny, nz);
        vertex.UV0 = FVector2(u * uvScale * size, v * uvScale * size);
        vertex.Tangent = FVector3(1.0f, 0.0f, 0.0f); // 简化处理，实际应根据法线计算切线
        const uint32_t index = static_cast<uint32_t>(mesh.Vertices.size());
        mesh.Vertices.push_back(vertex);
        return index;
    };
    // 定义立方体的 6 个面，每个面由两个三角形组成
    // 前面
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, -halfSize, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, -halfSize, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, -halfSize, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, -halfSize, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, halfSize, -halfSize, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(-halfSize, halfSize, -halfSize, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f));

	// 后面
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, halfSize, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, halfSize, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, halfSize, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, halfSize, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, halfSize, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(halfSize, halfSize, halfSize, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f));

	// 左面
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, halfSize, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, -halfSize, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, halfSize, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, -halfSize, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, -halfSize, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(-halfSize, halfSize, halfSize, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f));

	// 右面
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, -halfSize, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, halfSize, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, halfSize, -halfSize, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, halfSize, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	mesh.Indices.push_back(appendVertex(halfSize, halfSize, halfSize, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(halfSize, halfSize, -halfSize, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f));

	// 上面
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, -halfSize, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, halfSize, -halfSize, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, halfSize, halfSize, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
    mesh.Indices.push_back(appendVertex(halfSize, halfSize, -halfSize, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f));
	mesh.Indices.push_back(appendVertex(halfSize, halfSize, halfSize, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(-halfSize, halfSize, halfSize, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));

	// 下面
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, halfSize, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(halfSize, -halfSize, halfSize, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f));
    mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, -halfSize, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(halfSize, -halfSize, halfSize, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f));
	mesh.Indices.push_back(appendVertex(halfSize, -halfSize, -halfSize, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f));
	mesh.Indices.push_back(appendVertex(-halfSize, -halfSize, -halfSize, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f));

	FStaticMeshSection section = {};
    section.Name = "Box";
    section.MaterialIndex = 0;
    section.IndexStart = 0;
    section.IndexCount = static_cast<uint32_t>(mesh.Indices.size());
    section.VertexBase = 0;
    mesh.Sections.push_back(section);
    mesh.MaterialNames.push_back("Box");   // Box 构建器（原误写 "Ground"）
    mesh.RecomputeBounds();
	return mesh;
}
