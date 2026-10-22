#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "RHIInterfaces.h"
#include "DX12RHICommon.h"
#include "DX12RHIPipelineObjects.h"
#include "DX12RHIResource2.h"

#include "d3dx12.h"

#include "DXSampleHelper.h" // ThrowIfFailed + ReadDataFromFile

#include <cstdlib> // free
#include <vector>


class FDX12RHIDevice final : public IRHIDevice
{
public:
    explicit FDX12RHIDevice(ID3D12Device* InDevice)
        : Device(InDevice)
    {
    }

    std::shared_ptr<IRHIRootSignature> CreateRootSignature(const FRHIRootSignatureDesc& Desc) override
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[4];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, Desc.SrvCountSpace0, 0, 0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, Desc.SrvCountSpace1, 0, 1,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, Desc.SamplerCount, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, Desc.CbvCount, 0, 0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rootParameters[4];
        rootParameters[0].InitAsDescriptorTable(2, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[3].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        if (Desc.bAllowInputAssemblerInputLayout)
        {
            flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        }

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, flags);

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));

        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
        ThrowIfFailed(Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSig)));
        return std::make_shared<FDX12RHIRootSignature>(rootSig.Get());
    }

    std::shared_ptr<IRHIPipelineState> CreateGraphicsPipeline(const FRHIGraphicsPipelineDesc& Desc, IRHIRootSignature* RootSig) override
    {
        auto* dxRoot = dynamic_cast<FDX12RHIRootSignature*>(RootSig);
        if (!dxRoot)
        {
            ThrowIfFailed(E_FAIL);
        }

        UINT8* vsData = nullptr;
        UINT vsLen = 0;
        UINT8* psData = nullptr;
        UINT psLen = 0;
        ThrowIfFailed(ReadDataFromFile(Desc.VSPath.c_str(), &vsData, &vsLen));
        ThrowIfFailed(ReadDataFromFile(Desc.PSPath.c_str(), &psData, &psLen));

        CD3DX12_RASTERIZER_DESC raster(D3D12_DEFAULT);
        raster.CullMode = (Desc.Rasterizer.CullMode == ERHICullMode::None) ? D3D12_CULL_MODE_NONE :
            (Desc.Rasterizer.CullMode == ERHICullMode::Front) ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;

        CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
        (void)blend; // Future expansion.

        CD3DX12_DEPTH_STENCIL_DESC ds(D3D12_DEFAULT);
        ds.DepthEnable = Desc.DepthStencil.bDepthEnable ? TRUE : FALSE;
        ds.StencilEnable = Desc.DepthStencil.bStencilEnable ? TRUE : FALSE;

        // Vertex layout.
        std::vector<D3D12_INPUT_ELEMENT_DESC> input;
        input.reserve(Desc.VertexLayout.NumElements);
        for (uint32_t i = 0; i < Desc.VertexLayout.NumElements; ++i)
        {
            const auto& e = Desc.VertexLayout.Elements[i];
            D3D12_INPUT_ELEMENT_DESC d = {};
            d.SemanticName = e.SemanticName;
            d.SemanticIndex = e.SemanticIndex;
            d.Format = DX12_ToDXGIFormat(e.Format);
            d.InputSlot = e.InputSlot;
            d.AlignedByteOffset = e.AlignedByteOffset;
            d.InputSlotClass = e.bPerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            d.InstanceDataStepRate = e.InstanceDataStepRate;
            input.push_back(d);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { input.data(), static_cast<UINT>(input.size()) };
        psoDesc.pRootSignature = dxRoot->GetNative();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsData, vsLen);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(psData, psLen);
        psoDesc.RasterizerState = raster;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = ds;
        psoDesc.SampleMask = Desc.SampleMask;
        psoDesc.PrimitiveTopologyType = DX12_ToTopoType(Desc.TopologyType);
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DX12_ToDXGIFormat(Desc.RTVFormat);
        psoDesc.DSVFormat = DX12_ToDXGIFormat(Desc.DSVFormat);
        psoDesc.SampleDesc.Count = Desc.SampleCount;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        ThrowIfFailed(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

        free(vsData);
        free(psData);

        return std::make_shared<FDX12RHIPipelineState>(pso.Get());
    }

    std::shared_ptr<IRHIPipelineState> CreateComputePipeline(const FRHIComputePipelineDesc& Desc, IRHIRootSignature* RootSig) override
    {
        auto* dxRoot = dynamic_cast<FDX12RHIRootSignature*>(RootSig);
        if (!dxRoot)
        {
            ThrowIfFailed(E_FAIL);
        }

        UINT8* csData = nullptr;
        UINT csLen = 0;
        ThrowIfFailed(ReadDataFromFile(Desc.CSPath.c_str(), &csData, &csLen));

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = dxRoot->GetNative();
        psoDesc.CS = CD3DX12_SHADER_BYTECODE(csData, csLen);

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        ThrowIfFailed(Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

        free(csData);
        return std::make_shared<FDX12RHIPipelineState>(pso.Get());
    }

    std::shared_ptr<IRHITexture2D> CreateTexture2D(const FRHITexture2DDesc& Desc, ERHIResourceState InitialState, const FRHIClearColor* ClearColor) override
    {
        const DXGI_FORMAT format = DX12_ToDXGIFormat(Desc.Format);
        D3D12_RESOURCE_FLAGS flags = Desc.bAllowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;

        CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            format,
            Desc.Width,
            Desc.Height,
            1,
            static_cast<UINT16>(Desc.MipLevels),
            Desc.SampleCount,
            0,
            flags);

        D3D12_CLEAR_VALUE clear = {};
        D3D12_CLEAR_VALUE* pClear = nullptr;
        if (ClearColor && Desc.bAllowRenderTarget)
        {
            clear.Format = format;
            clear.Color[0] = ClearColor->Color[0];
            clear.Color[1] = ClearColor->Color[1];
            clear.Color[2] = ClearColor->Color[2];
            clear.Color[3] = ClearColor->Color[3];
            pClear = &clear;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> res;
        ThrowIfFailed(Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            DX12_ToD3DState(InitialState),
            pClear,
            IID_PPV_ARGS(&res)));

        return std::make_shared<FDX12RHITexture2D>(res.Get());
    }

private:
    ID3D12Device* Device = nullptr;
};
