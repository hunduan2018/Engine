#pragma once

#include <d3d12.h>

#include "RHIInterfaces.h"
#include "DX12RHICommon.h"
#include "DX12RHIPipelineObjects.h"
#include "DX12RHIResource2.h"
#include "d3dx12.h"


class FDX12RHICommandList final : public IRHICommandList
{
public:
    explicit FDX12RHICommandList(ID3D12GraphicsCommandList* InCmd)
        : Cmd(InCmd)
    {
    }

    void* GetNativeHandle() const override { return Cmd; }

    void RSSetViewports(uint32_t Count, const FRHIViewport* Viewports) override
    {
        // DX12 expects D3D12_VIEWPORT.
        // We only support Count==1 in this minimal adapter.
        if (!Cmd || Count == 0 || !Viewports)
        {
            return;
        }

        D3D12_VIEWPORT vp = {};
        vp.TopLeftX = Viewports[0].TopLeftX;
        vp.TopLeftY = Viewports[0].TopLeftY;
        vp.Width = Viewports[0].Width;
        vp.Height = Viewports[0].Height;
        vp.MinDepth = Viewports[0].MinDepth;
        vp.MaxDepth = Viewports[0].MaxDepth;
        Cmd->RSSetViewports(1, &vp);
    }

    void RSSetScissorRects(uint32_t Count, const FRHIRect* Rects) override
    {
        if (!Cmd || Count == 0 || !Rects)
        {
            return;
        }

        D3D12_RECT r = {};
        r.left = Rects[0].left;
        r.top = Rects[0].top;
        r.right = Rects[0].right;
        r.bottom = Rects[0].bottom;
        Cmd->RSSetScissorRects(1, &r);
    }

    void ResourceBarrier(IRHIResource* Resource, ERHIResourceState Before, ERHIResourceState After) override
    {
        if (!Cmd || !Resource)
        {
            return;
        }

        // Expect DX12 resources in this adapter.
        auto* dxRes = dynamic_cast<FDX12RHIResource*>(Resource);
        auto* dxTex = dynamic_cast<FDX12RHITexture2D*>(Resource);
        ID3D12Resource* native = dxRes ? dxRes->GetNative() : (dxTex ? dxTex->GetNative() : nullptr);
        if (!native)
        {
            return;
        }

        Cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(native, DX12_ToD3DState(Before), DX12_ToD3DState(After)));
    }

    void SetPipelineState(IRHIPipelineState* PSO) override
    {
        if (!Cmd || !PSO)
        {
            return;
        }
        auto* dx = dynamic_cast<FDX12RHIPipelineState*>(PSO);
        if (dx)
        {
            Cmd->SetPipelineState(dx->GetNative());
        }
    }

    void SetGraphicsRootSignature(IRHIRootSignature* RootSig) override
    {
        if (!Cmd || !RootSig)
        {
            return;
        }
        auto* dx = dynamic_cast<FDX12RHIRootSignature*>(RootSig);
        if (dx)
        {
            Cmd->SetGraphicsRootSignature(dx->GetNative());
        }
    }

    void SetDescriptorHeaps(IRHIDescriptorHeap* CbvSrvUavHeap, IRHIDescriptorHeap* SamplerHeap) override
    {
        if (!Cmd)
        {
            return;
        }

        ID3D12DescriptorHeap* heaps[2] = {};
        UINT count = 0;

        if (auto* dx = dynamic_cast<FDX12RHIDescriptorHeapView*>(CbvSrvUavHeap))
        {
            heaps[count++] = dx->GetNative();
        }
        if (auto* dx = dynamic_cast<FDX12RHIDescriptorHeapView*>(SamplerHeap))
        {
            heaps[count++] = dx->GetNative();
        }

        if (count > 0)
        {
            Cmd->SetDescriptorHeaps(count, heaps);
        }
    }


    void SetGraphicsRootDescriptorTable(uint32_t RootParameterIndex, FRHIGpuDescriptorHandle BaseDescriptor) override
    {
        if (!Cmd)
        {
            return;
        }
        Cmd->SetGraphicsRootDescriptorTable(RootParameterIndex, DX12_FromRHIGpu(BaseDescriptor));
    }

    void OMSetRenderTargets(uint32_t NumRTVs, const FRHICpuDescriptorHandle* RTVs, bool bRTVsSingleHandleToDescriptorRange,
        const FRHICpuDescriptorHandle* DSV) override
    {
        if (!Cmd)
        {
            return;
        }

        if (!RTVs || NumRTVs == 0)
        {
            Cmd->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
            return;
        }

        // Minimal: only supports NumRTVs==1.
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = DX12_FromRHICpu(RTVs[0]);
        const D3D12_CPU_DESCRIPTOR_HANDLE* pDsv = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
        if (DSV && DSV->ptr != 0)
        {
            dsv = DX12_FromRHICpu(*DSV);
            pDsv = &dsv;
        }

        Cmd->OMSetRenderTargets(1, &rtv, bRTVsSingleHandleToDescriptorRange ? TRUE : FALSE, pDsv);
    }

    void ClearRenderTargetView(FRHICpuDescriptorHandle RTV, const float ColorRGBA[4]) override
    {
        if (!Cmd)
        {
            return;
        }
        Cmd->ClearRenderTargetView(DX12_FromRHICpu(RTV), ColorRGBA, 0, nullptr);
    }

    void ClearDepthStencilView(FRHICpuDescriptorHandle DSV, float Depth, uint8_t Stencil) override
    {
        if (!Cmd)
        {
            return;
        }
        Cmd->ClearDepthStencilView(DX12_FromRHICpu(DSV), D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
    }

    void IASetPrimitiveTopology(ERHIPrimitiveTopology Topology) override
    {
        if (!Cmd)
        {
            return;
        }
        Cmd->IASetPrimitiveTopology(DX12_ToPrimitiveTopology(Topology));
    }

    void DrawInstanced(uint32_t VertexCountPerInstance, uint32_t InstanceCount, uint32_t StartVertexLocation, uint32_t StartInstanceLocation) override
    {
        if (!Cmd)
        {
            return;
        }
        Cmd->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
    }

    void ExecuteBundle(IRHICommandList* Bundle) override
    {
        if (!Cmd || !Bundle)
        {
            return;
        }
        auto* native = static_cast<ID3D12GraphicsCommandList*>(Bundle->GetNativeHandle());
        if (native)
        {
            Cmd->ExecuteBundle(native);
        }
    }

private:
    ID3D12GraphicsCommandList* Cmd = nullptr;
};
