#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "RHIInterfaces.h"

// Minimal DX12 resource wrapper. Owns a ref-counted reference via ComPtr.
class FDX12RHIResource : public IRHIResource
{
public:
    explicit FDX12RHIResource(ID3D12Resource* InResource)
        : Resource(InResource)
    {
    }

    ID3D12Resource* GetNative() const { return Resource.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
};

class FDX12RHITexture2D : public IRHITexture2D
{
public:
    explicit FDX12RHITexture2D(ID3D12Resource* InResource)
        : Resource(InResource)
    {
    }

    ID3D12Resource* GetNative() const { return Resource.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
};
