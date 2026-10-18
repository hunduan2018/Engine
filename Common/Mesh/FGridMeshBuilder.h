// FGridMeshBuilder.h
// 曲面细分演示网格：XZ 平面地面，每个三角形输出独立 3 顶点（一个三角形 = 一个
// 3 控制点 patch），供 3_CONTROL_POINT_PATCHLIST 拓扑绘制。

#pragma once

#include "FStaticMesh.h"

#include <cstdint>

class FGridMeshBuilder
{
public:
    // size：平面边长（世界单位）；divisions：每边格子数（每格 2 个三角形 patch）。
    static FStaticMesh BuildTessellatedPlane(float size, uint32_t divisions);
};
