// FVertexFactory.h
// 顶点布局工厂（UE5 FVertexFactory 的单后端裁剪）：描述一种顶点布局——
// 输入元素表（PSO InputLayout）+ 流数与每流步长（顶点缓冲视图构造依据）。
// "哪种网格用什么布局创建 PSO"由工厂表达；新增布局 = 新工厂类 + 注册，
// 枚举与缓存零改动（开闭原则）。
// 裁剪差异：UE5 工厂绑定具体缓冲且 per-LOD 实例化（Engine/Public/StaticMeshResources.h:746），
// 并在 SM5+ 走着色器 SRV 手动取顶点（LocalVertexFactory.cpp:451，IA 元素仅传统路径使用）；
// 本项目 v1 工厂是纯布局描述（全局注册表单例），SRV 取顶点留给第二阶段 MeshShading。

#pragma once

#include "RendererTypes.h"

#include <d3d12.h>

#include <cstdint>
#include <string>
#include <vector>

// 元素声明（工厂构造入参；SemanticName 须为静态存储期字面量）。
struct FVertexFactoryElement
{
    const char* SemanticName = "";
    uint32_t SemanticIndex = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    uint32_t StreamIndex = 0;
    uint32_t Offset = 0;
    bool bPerInstanceData = false;
};

class FVertexFactory
{
public:
    // 由子类提供元素表与每流步长；构造时汇编并按（流号, 偏移）排序
    //（对齐 UE5 RHICreateVertexDeclaration 的一次性构建，D3D12VertexDeclaration.cpp:83-98）。
    FVertexFactory(const std::vector<FVertexFactoryElement>& elements,
                   const uint32_t* streamStrides, uint32_t streamCount,
                   const std::string& name);
    virtual ~FVertexFactory() = default;

    FVertexFactory(const FVertexFactory&) = delete;
    FVertexFactory& operator=(const FVertexFactory&) = delete;

    // PSO 输入布局；元素已排序，数组随工厂存活。
    const D3D12_INPUT_ELEMENT_DESC* GetInputElementDescs(uint32_t& outCount) const;
    uint32_t GetStreamCount() const { return StreamCount; }
    uint32_t GetStreamStride(uint32_t streamIndex) const;
    const std::string& GetName() const { return Name; }

    // 注册表分配的布局身份（FRenderer::RegisterVertexFactory 填写；PSO 键携带）。
    FVertexFactoryHandle Handle = InvalidHandle;

private:
    std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
    uint32_t StreamStrides[RendererLimits::MaxVertexStreams] = {};
    uint32_t StreamCount = 0;
    std::string Name;
};
