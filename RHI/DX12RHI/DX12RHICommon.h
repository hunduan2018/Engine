#pragma once

#include <d3d12.h>
#include <dxgiformat.h>

#include "RHICommon.h"

inline DXGI_FORMAT DX12_ToDXGIFormat(ERHIFormat Fmt)
{
    switch (Fmt)
    {
    case ERHIFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ERHIFormat::D32_FLOAT: return DXGI_FORMAT_D32_FLOAT;
    case ERHIFormat::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
    case ERHIFormat::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

inline D3D12_RESOURCE_STATES DX12_ToD3DState(ERHIResourceState State)
{
    switch (State)
    {
    case ERHIResourceState::Present: return D3D12_RESOURCE_STATE_PRESENT;
    case ERHIResourceState::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ERHIResourceState::DepthWrite: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ERHIResourceState::PixelShaderResource: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case ERHIResourceState::NonPixelShaderResource: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case ERHIResourceState::Common: return D3D12_RESOURCE_STATE_COMMON;
    default: return D3D12_RESOURCE_STATE_COMMON;
    }
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE DX12_ToTopoType(ERHIPrimitiveTopologyType T)
{
    switch (T)
    {
    case ERHIPrimitiveTopologyType::Line: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case ERHIPrimitiveTopologyType::Point: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    default: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

inline D3D_PRIMITIVE_TOPOLOGY DX12_ToPrimitiveTopology(ERHIPrimitiveTopology T)
{
    switch (T)
    {
    case ERHIPrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case ERHIPrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    default: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

inline FRHICpuDescriptorHandle DX12_ToRHICpu(D3D12_CPU_DESCRIPTOR_HANDLE H)
{
    FRHICpuDescriptorHandle Out;
    Out.ptr = static_cast<uint64_t>(H.ptr);
    return Out;
}

inline FRHIGpuDescriptorHandle DX12_ToRHIGpu(D3D12_GPU_DESCRIPTOR_HANDLE H)
{
    FRHIGpuDescriptorHandle Out;
    Out.ptr = static_cast<uint64_t>(H.ptr);
    return Out;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE DX12_FromRHICpu(FRHICpuDescriptorHandle H)
{
    D3D12_CPU_DESCRIPTOR_HANDLE Out = {};
    Out.ptr = static_cast<SIZE_T>(H.ptr);
    return Out;
}

inline D3D12_GPU_DESCRIPTOR_HANDLE DX12_FromRHIGpu(FRHIGpuDescriptorHandle H)
{
    D3D12_GPU_DESCRIPTOR_HANDLE Out = {};
    Out.ptr = static_cast<UINT64>(H.ptr);
    return Out;
}
