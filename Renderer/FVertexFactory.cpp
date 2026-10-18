// FVertexFactory.cpp

#include "FVertexFactory.h"

#include "../src/Assert.h"

#include <algorithm>

FVertexFactory::FVertexFactory(const std::vector<FVertexFactoryElement>& elements,
                               const uint32_t* streamStrides, uint32_t streamCount,
                               const std::string& name)
    : StreamCount(streamCount)
    , Name(name)
{
    APP_CHECK_MSG(streamCount > 0 && streamCount <= RendererLimits::MaxVertexStreams,
        "VertexFactory stream count out of range");

    // 汇编 D3D12 元素：流号即 InputSlot，bPerInstanceData 决定输入分类
    //（转换规则对齐 UE5 D3D12VertexDeclaration.cpp:33-67）。
    InputElements.reserve(elements.size());
    for (const FVertexFactoryElement& element : elements)
    {
        APP_CHECK_MSG(element.StreamIndex < StreamCount, "Element stream index out of range");
        D3D12_INPUT_ELEMENT_DESC desc = {};
        desc.SemanticName = element.SemanticName;
        desc.SemanticIndex = element.SemanticIndex;
        desc.Format = element.Format;
        desc.InputSlot = element.StreamIndex;
        desc.AlignedByteOffset = element.Offset;
        desc.InputSlotClass = element.bPerInstanceData
            ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
            : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc.InstanceDataStepRate = element.bPerInstanceData ? 1 : 0;
        InputElements.push_back(desc);
    }

    // 按流号、偏移排序（驱动对未排序布局的行为未定义；UE5 同样排序）。
    std::sort(InputElements.begin(), InputElements.end(),
        [](const D3D12_INPUT_ELEMENT_DESC& a, const D3D12_INPUT_ELEMENT_DESC& b)
        {
            if (a.InputSlot != b.InputSlot)
            {
                return a.InputSlot < b.InputSlot;
            }
            return a.AlignedByteOffset < b.AlignedByteOffset;
        });

    for (uint32_t stream = 0; stream < streamCount; ++stream)
    {
        StreamStrides[stream] = streamStrides[stream];
    }
}

const D3D12_INPUT_ELEMENT_DESC* FVertexFactory::GetInputElementDescs(uint32_t& outCount) const
{
    outCount = static_cast<uint32_t>(InputElements.size());
    return InputElements.data();
}

uint32_t FVertexFactory::GetStreamStride(uint32_t streamIndex) const
{
    APP_CHECK(streamIndex < StreamCount);
    return StreamStrides[streamIndex];
}
