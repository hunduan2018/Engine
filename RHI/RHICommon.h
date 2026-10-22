#pragma once

#include <cstdint>

// NOTE: This header must stay RHI-agnostic (no D3D12/Vulkan/etc includes).

enum class ERHIFormat : uint8_t
{
    Unknown = 0,
    R8G8B8A8_UNORM,
    D32_FLOAT,
    R32G32B32_FLOAT,
    R32G32_FLOAT,
};

enum class ERHIResourceState : uint16_t
{
    Common = 0,
    Present,
    RenderTarget,
    DepthWrite,
    PixelShaderResource,
    NonPixelShaderResource,
};

enum class ERHICullMode : uint8_t
{
    None = 0,
    Front,
    Back,
};

enum class ERHIPrimitiveTopologyType : uint8_t
{
    Triangle = 0,
    Line,
    Point,
};

enum class ERHIPrimitiveTopology : uint8_t
{
    TriangleList = 0,
    LineList,
    PointList,
};

struct FRHIViewport
{
    float TopLeftX = 0.0f;
    float TopLeftY = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    float MinDepth = 0.0f;
    float MaxDepth = 1.0f;
};

struct FRHIRect
{
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
};

// Opaque descriptor handles.
struct FRHICpuDescriptorHandle
{
    uint64_t ptr = 0;
};

struct FRHIGpuDescriptorHandle
{
    uint64_t ptr = 0;
};

struct FRHIClearColor
{
    float Color[4] = { 0, 0, 0, 1 };
};

struct FRHIClearDepthStencil
{
    float Depth = 1.0f;
    uint8_t Stencil = 0;
};
