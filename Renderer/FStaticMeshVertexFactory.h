// FStaticMeshVertexFactory.h
// 静态网格默认工厂：44 字节单流交错布局
//（POSITION@0 / NORMAL@12 / TEXCOORD@24 / TANGENT@32，与 FStaticMeshVertex 对齐）。
// 布局定义迁自 FStaticMesh::InputLayout()——布局归属渲染层工厂，网格容器不再感知 IA。

#pragma once

#include "FVertexFactory.h"

class FStaticMeshVertexFactory : public FVertexFactory
{
public:
    FStaticMeshVertexFactory();
};
