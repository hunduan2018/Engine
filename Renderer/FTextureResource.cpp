// FTextureResource.cpp

#include "FTextureResource.h"

#include "DescriptorHeapManagement.h"
#include "FResourceUploadContext.h"
#include "../src/Assert.h"
#include "d3dUtil.h"
#include "d3dx12.h"

#include <vector>

// 通用纹理创建路径：默认堆纹理 + 单一 mip 上传 + 像素着色器资源屏障 + bindless SRV。
static FTextureResource CreateTextureInternal(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    DXGI_FORMAT format,
    const uint8_t* data,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes,
    BindlessAllocator& bindless,
    FResourceUploadContext& uploadContext)
{
    FTextureResource textureResource;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        format, width, height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_NONE);
    D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE,
        &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&textureResource.Texture)));

    // 上传堆缓冲：按 GPU 要求的中间尺寸分配，存活期交给 uploadContext。
    const UINT64 uploadSize = GetRequiredIntermediateSize(textureResource.Texture.Get(), 0, 1);
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    {
        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
        D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE,
            &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&uploadBuffer)));
    }

    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = data;
    subresource.RowPitch = static_cast<LONG_PTR>(rowPitchBytes);
    subresource.SlicePitch = static_cast<LONG_PTR>(rowPitchBytes) * height;

    UpdateSubresources<1>(commandList, textureResource.Texture.Get(), uploadBuffer.Get(), 0, 0, 1, &subresource);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        textureResource.Texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);

    // SRV 写入 bindless 堆槽位。堆槽 0 由 FRenderer 预留给实例缓冲，
    // 纹理必须从 FirstTextureBindlessSlot 起分配，否则数组索引会下溢。
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = {};
    const uint32_t heapSlot = bindless.Allocate(srvCpuHandle);
    APP_CHECK(heapSlot >= RendererLimits::FirstTextureBindlessSlot);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(textureResource.Texture.Get(), &srvDesc, srvCpuHandle);

    uploadContext.AddUploadBuffer(std::move(uploadBuffer));

    textureResource.Width = width;
    textureResource.Height = height;
    textureResource.SrvArrayIndex = heapSlot - RendererLimits::FirstTextureBindlessSlot;
    return textureResource;
}

FTextureResource CreateTextureFromBC1(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    const uint8_t* data,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes,
    BindlessAllocator& bindless,
    FResourceUploadContext& uploadContext)
{
    return CreateTextureInternal(
        device, commandList, DXGI_FORMAT_BC1_UNORM, data, width, height, rowPitchBytes,
        bindless, uploadContext);
}

FTextureResource CreateCheckerTexture(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    uint32_t width,
    uint32_t height,
    uint32_t checkerSize,
    BindlessAllocator& bindless,
    FResourceUploadContext& uploadContext)
{
    // 行间距按 256 字节对齐（D3D12_TEXTURE_DATA_PITCH_ALIGNMENT）。
    const uint32_t rowPitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
        & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    std::vector<uint8_t> pixels(static_cast<size_t>(rowPitch) * height, 0);

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const bool bWhiteTile = (((x / checkerSize) + (y / checkerSize)) & 1) != 0;
            uint8_t* pixel = &pixels[static_cast<size_t>(y) * rowPitch + x * 4];
            pixel[0] = bWhiteTile ? 220 : 32;
            pixel[1] = bWhiteTile ? 220 : 32;
            pixel[2] = bWhiteTile ? 235 : 90;
            pixel[3] = 255;
        }
    }

    return CreateTextureInternal(
        device, commandList, DXGI_FORMAT_R8G8B8A8_UNORM, pixels.data(), width, height, rowPitch,
        bindless, uploadContext);
}
