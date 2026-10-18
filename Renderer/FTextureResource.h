// FTextureResource.h
// GPU 纹理资源：默认堆纹理 + bindless SRV 槽位。
// 上传走 FResourceUploadContext（初始化期一次性完成）。

#pragma once

#include "RendererTypes.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>

class BindlessAllocator;
class FResourceUploadContext;

struct FTextureResource
{
    Microsoft::WRL::ComPtr<ID3D12Resource> Texture;
    uint32_t Width = 0;
    uint32_t Height = 0;

    // bindless 数组索引 = 堆槽位 - FirstTextureBindlessSlot；着色器用它索引 g_BindlessTextures[]。
    uint32_t SrvArrayIndex = 0;

    bool IsValid() const { return Texture != nullptr; }
};

// 从内存中的 BC1 数据块创建（如 occcity.bin 内嵌的 city.dds 像素数据）。
// data 为紧排的 BC1 块数据，rowPitchBytes 为一行压缩块数据的字节间距。
FTextureResource CreateTextureFromBC1(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    const uint8_t* data,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes,
    BindlessAllocator& bindless,
    FResourceUploadContext& uploadContext);

// 创建程序生成的 RGBA8 棋盘格纹理（第二演示材质用，尺寸 tileSizes 可调）。
FTextureResource CreateCheckerTexture(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    uint32_t width,
    uint32_t height,
    uint32_t checkerSize,
    BindlessAllocator& bindless,
    FResourceUploadContext& uploadContext);
