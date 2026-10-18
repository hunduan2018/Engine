// FMeshPassDrawList.h
// 每渲染通道的命令桶：命令向量 + 排序索引。纯 CPU 内存，每帧 BeginFrame 重建，
// Reset 保留容量避免反复分配。

#pragma once

#include "FMeshDrawCommand.h"

#include <algorithm>
#include <cstdint>
#include <vector>

class FMeshPassDrawList
{
public:
    // 追加命令，返回序列号（调用方写入 SortKey 低 16 位）。
    uint32_t Add(const FMeshDrawCommand& command)
    {
        Commands.push_back(command);
        return static_cast<uint32_t>(Commands.size() - 1);
    }

    // 按 SortKey 稳定排序（同键保持提交顺序）。
    void Sort()
    {
        SortedIndices.resize(Commands.size());
        for (uint32_t i = 0; i < Commands.size(); ++i)
        {
            SortedIndices[i] = i;
        }
        std::stable_sort(SortedIndices.begin(), SortedIndices.end(),
            [this](uint32_t a, uint32_t b)
            {
                return Commands[a].SortKey < Commands[b].SortKey;
            });
    }

    // 每帧重置；clear 保留容量。
    void Reset()
    {
        Commands.clear();
        SortedIndices.clear();
    }

    uint32_t Num() const { return static_cast<uint32_t>(Commands.size()); }
    const FMeshDrawCommand& GetCommand(uint32_t sortedIndex) const { return Commands[SortedIndices[sortedIndex]]; }

private:
    std::vector<FMeshDrawCommand> Commands;
    std::vector<uint32_t> SortedIndices;
};

inline uint64_t ComposeOpaqueSortKey(uint16_t passId, uint16_t psoIndex, uint16_t materialId, uint16_t sequence)
{
    return (static_cast<uint64_t>(passId) << 48)
        | (static_cast<uint64_t>(psoIndex) << 32)
        | (static_cast<uint64_t>(materialId) << 16)
        | static_cast<uint64_t>(sequence);
}
