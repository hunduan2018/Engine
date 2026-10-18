// FGlobalRootSignature.h
// v1 全局唯一根签名，所有绘制（含曲面细分变体）共享：
//   [0] CBV  b0  视图常量（FViewConstants）
//   [1] CBV  b1  材质常量（FMaterialConstants）
//   [2] 4x32bit 常量 b2 绘制根常量（InstanceOffset + 3 个预留，预留给 MeshShading 的缓冲 SRV 索引）
//   [3] SRV 描述符表 t0..t0+BindlessSrvCount-1  bindless 堆（t0=实例缓冲，t1..=纹理）
//   静态采样器 s0（线性/Wrap/各向异性16）、s1（点/Clamp），无需采样器堆。
// 单一签名的收益：零根签名切换（最贵状态变更）、PSO 全部同签名复用、
// 材质差异完全由根参数值 + bindless 索引表达（UE5 全局 bindless 模型）。

#pragma once

#include <d3d12.h>
#include <wrl.h>

// 根参数槽位（着色器寄存器与本类的对应关系见文件头注释）。
namespace RootParam
{
    constexpr uint32_t ViewCBV = 0;             // b0
    constexpr uint32_t MaterialCBV = 1;         // b1
    constexpr uint32_t DrawConstants = 2;       // b2
    constexpr uint32_t BindlessSrvTable = 3;    // t0..
    constexpr uint32_t Count = 4;
}

class FGlobalRootSignature
{
public:
    // 创建根签名（RS 1.1；设备不支持 1.1 时内部回退 1.0）。
    void Initialize(ID3D12Device* device);

    ID3D12RootSignature* Get() const { return RootSignature.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
};
