// FMaterialConstantCache.cpp

#include "FMaterialConstantCache.h"

#include <cstring>

D3D12_GPU_VIRTUAL_ADDRESS FMaterialConstantCache::GetOrCreate(const FMaterial* material,
                                                              FUploadConstantRing& ring,
                                                              uint32_t frameSlot)
{
    for (const auto& entry : Entries)
    {
        if (entry.first == material)
        {
            return entry.second;
        }
    }

    FMaterialConstants materialConstants = {};
    materialConstants.BaseColor = material->BaseColor;
    materialConstants.DiffuseSrvIndex = material->DiffuseSrvArrayIndex;
    materialConstants.TessellationFactor = material->TessellationFactor;

    const FRingAllocation allocation = ring.Alloc(frameSlot, sizeof(FMaterialConstants));
    std::memcpy(allocation.CpuAddress, &materialConstants, sizeof(materialConstants));

    Entries.emplace_back(material, allocation.GpuAddress);
    return allocation.GpuAddress;
}
