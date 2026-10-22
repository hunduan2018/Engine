#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "RHIInterfaces.h"
#include "DX12RHICommon.h"


class FDX12RHIRootSignature final : public IRHIRootSignature
{
public:
    explicit FDX12RHIRootSignature(ID3D12RootSignature* In)
        : RootSig(In)
    {
    }

    ID3D12RootSignature* GetNative() const { return RootSig.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSig;
};

class FDX12RHIPipelineState final : public IRHIPipelineState
{
public:
    explicit FDX12RHIPipelineState(ID3D12PipelineState* In)
        : PSO(In)
    {
    }

    ID3D12PipelineState* GetNative() const { return PSO.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO;
};

class FDX12RHIDescriptorHeapView final : public IRHIDescriptorHeap
{
public:
    explicit FDX12RHIDescriptorHeapView(ID3D12DescriptorHeap* In)
        : Heap(In)
    {
    }

    FRHIGpuDescriptorHandle GetGPUDescriptorHandleForHeapStart() const override
    {
        if (!Heap)
        {
            return {};
        }
        return DX12_ToRHIGpu(Heap->GetGPUDescriptorHandleForHeapStart());
    }

    ID3D12DescriptorHeap* GetNative() const { return Heap.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap;
};
