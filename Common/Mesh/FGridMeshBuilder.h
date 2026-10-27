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
// inline 变量：多编译单元包含本头文件时链接器合并为一份定义（否则 LNK2005 重定义）。
inline FStaticMesh GroundStaticMesh = FGridMeshBuilder::BuildTessellatedPlane(220.0f, 11);
class FBoxMeshBuilder
{
    public:
    // size：立方体边长（世界单位）。
    static FStaticMesh BuildBox(float size);
};

inline FStaticMesh BoxStaticMesh = FBoxMeshBuilder::BuildBox(100.0f);
