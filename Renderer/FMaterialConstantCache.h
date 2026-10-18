// FMaterialConstantCache.h
// 每帧材质常量地址缓存：同一材质的多个批次共享一次上传环分配（一次写入、一处地址）。
// 收益有二：避免同材质重复写常量；b1 地址稳定使 FDrawSubmitter 的根绑定去重生效
//（材质 CB 的 Set 次数 = 材质数，而非批次数）。对应 UE5"uniform buffer 一次创建、
// 多处绑定"的用法。

#pragma once

#include "FMaterial.h"
#include "FUploadConstantRing.h"

#include <d3d12.h>

#include <utility>
#include <vector>

class FMaterialConstantCache
{
public:
    // 每帧调用（BeginFrame 中）。
    void Reset() { Entries.clear(); }

    // 命中返回已分配地址；未命中写入上传环并缓存。
    D3D12_GPU_VIRTUAL_ADDRESS GetOrCreate(const FMaterial* material,
                                          FUploadConstantRing& ring,
                                          uint32_t frameSlot);

private:
    // 材质数量少（< 百级），线性查找优于哈希表的内存与缓存开销。
    std::vector<std::pair<const FMaterial*, D3D12_GPU_VIRTUAL_ADDRESS>> Entries;
};
