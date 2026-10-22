//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12DynamicIndexing.h"
#include "occcity.h"
#include "../Common/D3D12QueueManger.h"

#include "DX12RHIDevice.h"
#include "DX12RHICommandList.h"
#include "DX12RHICommon.h"
#include "DX12RHIResource2.h"
#include "DX12RenderPipelinePasses.h"


#include <cstdlib> // free


const float D3D12DynamicIndexing::CitySpacingInterval = 16.0f;

D3D12DynamicIndexing::D3D12DynamicIndexing(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_frameCounter(0),
    m_fenceValue(0),
    m_rtvDescriptorSize(0),
    m_cbvSrvDescriptorSize(0),
    m_currentFrameResourceIndex(0),
    m_pCurrentFrameResource(nullptr),
    mQueueManager(nullptr)
{
}

void D3D12DynamicIndexing::OnInit()
{
    m_camera.Init({ (CityColumnCount / 2.0f) * CitySpacingInterval - (CitySpacingInterval / 2.0f), 15, 50 });
    m_camera.SetMoveSpeed(CitySpacingInterval * 2.0f);

    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3D12DynamicIndexing::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    if (mQueueManager == nullptr)
    {
        mQueueManager = std::make_unique<Direct3DQueueManager>(m_device.Get());
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    m_commandQueue = mQueueManager->GetGraphicsQueue();
    //ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    //NAME_D3D12_OBJECT(m_commandQueue);

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue->Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        // +1 for an extra offscreen RTV (SceneColor) used by the postprocess chain.
        m_rtvDescriptorHeap = std::make_unique<DescriptorHeap>(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, FrameCount + 1, false);


        // Describe and create a depth stencil view (DSV) descriptor heap.
        m_dsvDescriptorHeap = std::make_unique<DescriptorHeap>(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

        // Describe and create a shader resource view (SRV) and constant 
        // buffer view (CBV) descriptor heap.
        m_cbvSrvDescriptorHeap = std::make_unique<DescriptorHeap>(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100000,true);

		m_cbvSrvDescriptorHeap->SetName(L"CBV_SRV_UAV_Descriptor_Heap");

        // Describe and create a sampler descriptor heap.
        m_samplerDescriptorHeap = std::make_unique<DescriptorHeap>(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 4080, true);
        m_samplerDescriptorHeap->SetName(L"Sampler_Descriptor_Heap");

        m_rtvDescriptorSize = m_rtvDescriptorHeap->GetDescriptorSize();
        m_cbvSrvDescriptorSize = m_cbvSrvDescriptorHeap->GetDescriptorSize();
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12DynamicIndexing::LoadAssets()
{
    // Note: ComPtr's are CPU objects but these resources need to stay in scope until
    // the command list that references them has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resources are not
    // prematurely destroyed.
    ComPtr<ID3D12Resource> vertexBufferUploadHeap;
    ComPtr<ID3D12Resource> indexBufferUploadHeap;
    ComPtr<ID3D12Resource> textureUploadHeap;
    ComPtr<ID3D12Resource> materialsUploadHeap;

    // Create configurable pipeline objects (RHI-agnostic front-end + DX12RHI back-end) and the pass chain.
    {
        auto rhiDevice = std::make_shared<FDX12RHIDevice>(m_device.Get());
        m_renderPipeline = std::make_unique<FRenderPipeline>(rhiDevice);

        // Root signature configuration (keeps compatibility with existing shader bindings).
        FRHIRootSignatureDesc rsDesc;
        rsDesc.SrvCountSpace0 = 5000;
        rsDesc.SrvCountSpace1 = 5000;
        rsDesc.SamplerCount = 4080;
        rsDesc.CbvCount = 1;
        rsDesc.bAllowInputAssemblerInputLayout = true;
        m_renderPipeline->RegisterRootSignature(L"CityRS", rsDesc);

        static const FRHIVertexElement kCityVertexLayout[] = {
            { "POSITION", 0, ERHIFormat::R32G32B32_FLOAT, 0, 0, false, 0 },
            { "NORMAL",   0, ERHIFormat::R32G32B32_FLOAT, 0, 12, false, 0 },
            { "TEXCOORD", 0, ERHIFormat::R32G32_FLOAT,    0, 24, false, 0 },
            { "TANGENT",  0, ERHIFormat::R32G32B32_FLOAT, 0, 32, false, 0 },
        };
        const FRHIVertexLayout cityLayout = { kCityVertexLayout, _countof(kCityVertexLayout) };

        // Graphics PSO configuration (loaded from .cso so it is data-driven).
        FRHIGraphicsPipelineDesc cityGp;
        cityGp.RootSignatureName = L"CityRS";
        cityGp.VSPath = GetAssetFullPath(L"Shaders\\shader_mesh_simple_vert.cso");
        cityGp.PSPath = GetAssetFullPath(L"Shaders\\shader_mesh_dynamic_indexing_pixel.cso");
        cityGp.VertexLayout = cityLayout;
        cityGp.Rasterizer.CullMode = ERHICullMode::None;
        cityGp.RTVFormat = ERHIFormat::R8G8B8A8_UNORM;
        cityGp.DSVFormat = ERHIFormat::D32_FLOAT;
        cityGp.SampleCount = 1;
        m_renderPipeline->RegisterGraphicsPipeline(L"CityGP", cityGp);

        // Post-process copy PSO (SceneColor -> BackBuffer).
        FRHIGraphicsPipelineDesc postGp;
        postGp.RootSignatureName = L"CityRS";
        postGp.VSPath = GetAssetFullPath(L"Shaders\\shader_fullscreen_vert.cso");
        postGp.PSPath = GetAssetFullPath(L"Shaders\\shader_postprocess_copy_pixel.cso");
        postGp.VertexLayout = { nullptr, 0 };
        postGp.Rasterizer.CullMode = ERHICullMode::None;
        postGp.DepthStencil.bDepthEnable = false;
        postGp.DepthStencil.bStencilEnable = false;
        postGp.RTVFormat = ERHIFormat::R8G8B8A8_UNORM;
        postGp.DSVFormat = ERHIFormat::Unknown;
        postGp.SampleCount = 1;
        m_renderPipeline->RegisterGraphicsPipeline(L"PostCopyGP", postGp);

        // Default configurable chain:
        //   SceneColor (geometry) -> PostProcess (fullscreen) -> Present.
        const FRHICpuDescriptorHandle sceneColorRtv = DX12_ToRHICpu(m_rtvDescriptorHeap->GetCpuHandle(FrameCount));
        m_renderPipeline->ClearPasses();
        m_renderPipeline->AddPass(std::make_unique<FDX12BeginTextureRenderTargetPass>(L"SceneColor", sceneColorRtv));
        m_renderPipeline->AddPass(std::make_unique<FDX12CityGeometryPass>(L"CityGP", L"CityRS"));
        m_renderPipeline->AddPass(std::make_unique<FDX12PostProcessCopyPass>(L"PostCopyGP", L"CityRS", L"SceneColor"));
        m_renderPipeline->AddPass(std::make_unique<FDX12EndBackbufferPass>());
    }



    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    NAME_D3D12_OBJECT(m_commandList);

    // Create render target views (RTVs).
    //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCpuHandle(0));//->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FrameCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        NAME_D3D12_OBJECT_INDEXED(m_renderTargets, i);
    }

    // Create offscreen SceneColor (RTV + SRV at index 0) for postprocess.
    {
        FRHITexture2DDesc sceneCfg;
        sceneCfg.Width = m_width;
        sceneCfg.Height = m_height;
        sceneCfg.Format = ERHIFormat::R8G8B8A8_UNORM;
        sceneCfg.bAllowRenderTarget = true;
        sceneCfg.MipLevels = 1;
        sceneCfg.SampleCount = 1;

        FRHIClearColor clearColor;
        clearColor.Color[0] = 0.0f;
        clearColor.Color[1] = 0.2f;
        clearColor.Color[2] = 0.4f;
        clearColor.Color[3] = 1.0f;

        m_renderPipeline->RegisterTexture2D(L"SceneColor", sceneCfg, ERHIResourceState::RenderTarget, &clearColor);
        auto sceneColorTex = m_renderPipeline->GetTexture2D(L"SceneColor");
        auto* dxSceneColor = dynamic_cast<FDX12RHITexture2D*>(sceneColorTex.get());
        ID3D12Resource* sceneColor = dxSceneColor ? dxSceneColor->GetNative() : nullptr;
        if (!sceneColor)
        {
            ThrowIfFailed(E_FAIL);
        }

        // RTV occupies the extra slot in the RTV heap.
        m_device->CreateRenderTargetView(sceneColor, nullptr, m_rtvDescriptorHeap->GetCpuHandle(FrameCount));

        // SRV is placed at CBV/SRV/UAV heap index 0 (t0), reserved for postprocess input.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(sceneColor, &srvDesc, m_cbvSrvDescriptorHeap->GetCpuHandle(0));
    }


    // Read in mesh data for vertex/index buffers.

    UINT8* pMeshData;
    UINT meshDataLength;
    ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(SampleAssets::DataFileName).c_str(), &pMeshData, &meshDataLength));

    // Create the vertex buffer.
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(SampleAssets::VertexDataSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(SampleAssets::VertexDataSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBufferUploadHeap)));

        NAME_D3D12_OBJECT(m_vertexBuffer);

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the vertex buffer.
        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = pMeshData + SampleAssets::VertexDataOffset;
        vertexData.RowPitch = SampleAssets::VertexDataSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        UpdateSubresources<1>(m_commandList.Get(), m_vertexBuffer.Get(), vertexBufferUploadHeap.Get(), 0, 0, 1, &vertexData);
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = SampleAssets::StandardVertexStride;
        m_vertexBufferView.SizeInBytes = SampleAssets::VertexDataSize;
    }

    // Create the index buffer.
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(SampleAssets::IndexDataSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_indexBuffer)));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(SampleAssets::IndexDataSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexBufferUploadHeap)));

        NAME_D3D12_OBJECT(m_indexBuffer);

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the index buffer.
        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = pMeshData + SampleAssets::IndexDataOffset;
        indexData.RowPitch = SampleAssets::IndexDataSize;
        indexData.SlicePitch = indexData.RowPitch;

        UpdateSubresources<1>(m_commandList.Get(), m_indexBuffer.Get(), indexBufferUploadHeap.Get(), 0, 0, 1, &indexData);
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

        // Describe the index buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = SampleAssets::StandardIndexFormat;
        m_indexBufferView.SizeInBytes = SampleAssets::IndexDataSize;

        m_numIndices = SampleAssets::IndexDataSize / 4;    // R32_UINT (SampleAssets::StandardIndexFormat) = 4 bytes each.
    }

    // Create the textures and sampler.
    {
        // Procedurally generate an array of textures to use as city materials.
        {
            // All of these materials use the same texture desc.
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = CityMaterialTextureWidth;
            textureDesc.Height = CityMaterialTextureHeight;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            // The textures evenly span the color rainbow so that each city gets
            // a different material.
            float materialGradStep = (1.0f / static_cast<float>(CityMaterialCount));

            // Generate texture data.
            std::vector<std::vector<unsigned char>> cityTextureData;
            cityTextureData.resize(CityMaterialCount);
            for (UINT i = 0; i < CityMaterialCount; ++i)
            {
                ThrowIfFailed(m_device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                    D3D12_HEAP_FLAG_NONE,
                    &textureDesc,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(&m_cityMaterialTextures[i])));

                NAME_D3D12_OBJECT_INDEXED(m_cityMaterialTextures, i);

                // Fill the texture.
                float t = i * materialGradStep;
                cityTextureData[i].resize(CityMaterialTextureWidth * CityMaterialTextureHeight * CityMaterialTextureChannelCount);
                for (int x = 0; x < CityMaterialTextureWidth; ++x)
                {
                    for (int y = 0; y < CityMaterialTextureHeight; ++y)
                    {
                        // Compute the appropriate index into the buffer based on the x/y coordinates.
                        int pixelIndex = (y * CityMaterialTextureChannelCount * CityMaterialTextureWidth) + (x * CityMaterialTextureChannelCount);

                        // Determine this row's position along the rainbow gradient.
                        float tPrime = t + ((static_cast<float>(y) / static_cast<float>(CityMaterialTextureHeight)) * materialGradStep);

                        // Compute the RGB value for this position along the rainbow
                        // and pack the pixel value.
                        FSimdVector hsl = XMVectorSet(tPrime, 0.5f, 0.5f, 1.0f);
                        FSimdVector rgb = XMColorHSLToRGB(hsl);

                        cityTextureData[i][pixelIndex + 0] = static_cast<unsigned char>((255 * XMVectorGetX(rgb)));
                        cityTextureData[i][pixelIndex + 1] = static_cast<unsigned char>((255 * XMVectorGetY(rgb)));
                        cityTextureData[i][pixelIndex + 2] = static_cast<unsigned char>((255 * XMVectorGetZ(rgb)));
                        cityTextureData[i][pixelIndex + 3] = 255;
                    }
                }
            }

            // Upload texture data to the default heap resources.
            {
                const UINT subresourceCount = textureDesc.DepthOrArraySize * textureDesc.MipLevels;
                const UINT64 uploadBufferStep = GetRequiredIntermediateSize(m_cityMaterialTextures[0].Get(), 0, subresourceCount); // All of our textures are the same size in this case.
                const UINT64 uploadBufferSize = uploadBufferStep * CityMaterialCount;
                ThrowIfFailed(m_device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&materialsUploadHeap)));

                for (int i = 0; i < CityMaterialCount; ++i)
                {
                    // Copy data to the intermediate upload heap and then schedule 
                    // a copy from the upload heap to the appropriate texture.
                    D3D12_SUBRESOURCE_DATA textureData = {};
                    textureData.pData = &cityTextureData[i][0];
                    textureData.RowPitch = static_cast<LONG_PTR>((CityMaterialTextureChannelCount * textureDesc.Width));
                    textureData.SlicePitch = textureData.RowPitch * textureDesc.Height;

                    UpdateSubresources(m_commandList.Get(), m_cityMaterialTextures[i].Get(), materialsUploadHeap.Get(), i * uploadBufferStep, 0, subresourceCount, &textureData);
                    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_cityMaterialTextures[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
                }
            }
        }

        // Load the occcity diffuse texture with baked-in ambient lighting.
        // This texture will be blended with a texture from the materials
        // array in the pixel shader.
        {
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = SampleAssets::Textures[0].MipLevels;
            textureDesc.Format = SampleAssets::Textures[0].Format;
            textureDesc.Width = SampleAssets::Textures[0].Width;
            textureDesc.Height = SampleAssets::Textures[0].Height;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_cityDiffuseTexture)));

            const UINT subresourceCount = textureDesc.DepthOrArraySize * textureDesc.MipLevels;
            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_cityDiffuseTexture.Get(), 0, subresourceCount);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&textureUploadHeap)));

            NAME_D3D12_OBJECT(m_cityDiffuseTexture);

            // Copy data to the intermediate upload heap and then schedule 
            // a copy from the upload heap to the diffuse texture.
            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData = pMeshData + SampleAssets::Textures[0].Data[0].Offset;
            textureData.RowPitch = SampleAssets::Textures[0].Data[0].Pitch;
            textureData.SlicePitch = SampleAssets::Textures[0].Data[0].Size;

            UpdateSubresources(m_commandList.Get(), m_cityDiffuseTexture.Get(), textureUploadHeap.Get(), 0, 0, subresourceCount, &textureData);
            m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_cityDiffuseTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        }

		// 假设我们有一个简单的结构体
		struct  ConstData
		{
			float bar[4];
		};

		// 假设我们有一些不定长的数据集
		std::vector<std::vector<ConstData>> dataSets = {
			{ {100.0f, 250.0f, 3.0f, 4.0f}, {250.0f, 60.0f, 7.0f, 8.0f} },
			{ {69.0f, 150.0f, 11.0f, 12.0f} },
            { {190.0f, 210.0f, 11.0f, 13.0f},{19.0f, 11.0f, 11.0f, 13.0f},{19.0f, 11.0f, 11.0f, 13.0f} },
            { {129.0f, 120.0f, 11.0f, 13.0f},{19.0f, 11.0f, 11.0f, 13.0f} }
			// 更多数据集...
		};
        StructBufferNum = dataSets.size();
        // SRV
        {
            
			//std::vector<ID3D12Resource*> structuredBuffers(dataSets.size());
			std::vector<ID3D12Resource*> uploadBuffers(dataSets.size());

			for (size_t i = 0; i < dataSets.size(); ++i) {
				size_t elementCount = dataSets[i].size();
				size_t bufferSize = sizeof(ConstData) * elementCount;

				// 创建 StructuredBuffer
				CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
				CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

                m_device->CreateCommittedResource(
					&defaultHeapProps,
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&m_cityMaterialStructures[i]));

				// 创建上传堆
				CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
                m_device->CreateCommittedResource(
					&uploadHeapProps,
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&uploadBuffers[i]));
			}

			for (size_t i = 0; i < dataSets.size(); ++i) {
				void* mappedData;
				CD3DX12_RANGE readRange(0, 0);
				uploadBuffers[i]->Map(0, &readRange, &mappedData);
				memcpy(mappedData, dataSets[i].data(), sizeof(ConstData) * dataSets[i].size());
				uploadBuffers[i]->Unmap(0, nullptr);
			}


            // 使用命令列表将数据从上传堆复制到 GPU 的 StructuredBuffer
			for (size_t i = 0; i < dataSets.size(); ++i) {
                m_commandList->CopyBufferRegion(m_cityMaterialStructures[i].Get(), 0, uploadBuffers[i], 0, sizeof(ConstData) * dataSets[i].size());
			}

            StructBufferOffset = 5000;// 1 + CityMaterialCount;

			// 创建每个 StructuredBuffer 的 SRV
            UINT i = 0;
			for (; i < dataSets.size(); ++i) {
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = dataSets[i].size();
				srvDesc.Buffer.StructureByteStride = sizeof(ConstData);
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;

                D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cbvSrvDescriptorHeap->GetCpuHandle(StructBufferOffset + i);// (m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), StructBufferOffset + i, m_cbvSrvDescriptorSize);
				m_device->CreateShaderResourceView(m_cityMaterialStructures[i].Get(), &srvDesc, handle);
			}
        }

        // Describe and create a sampler.
        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        m_device->CreateSampler(&samplerDesc, m_samplerDescriptorHeap->GetCpuHandle(0));

        // Create SRV for the city's diffuse texture.
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_cbvSrvDescriptorHeap->GetCpuHandle(1);// (m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_cbvSrvDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC diffuseSrvDesc = {};
        diffuseSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        diffuseSrvDesc.Format = SampleAssets::Textures->Format;
        diffuseSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        diffuseSrvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_cityDiffuseTexture.Get(), &diffuseSrvDesc, srvHandle);
        
        srvHandle = m_cbvSrvDescriptorHeap->GetCpuHandle(2);// Offset(m_cbvSrvDescriptorSize);

        // Create SRVs for each city material.
        for (int i = 0; i < CityMaterialCount; ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC materialSrvDesc = {};
            materialSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            materialSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            materialSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            materialSrvDesc.Texture2D.MipLevels = 1;
            m_device->CreateShaderResourceView(m_cityMaterialTextures[i].Get(), &materialSrvDesc, srvHandle);

            //srvHandle.Offset(m_cbvSrvDescriptorSize);
            srvHandle = m_cbvSrvDescriptorHeap->GetCpuHandle(i + 2);
        }
    }

    free(pMeshData);

    // Create the depth stencil view.
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE), // Performance tip: Deny shader resource access to resources that don't need shader resource views.
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0), // Performance tip: Tell the runtime at resource creation the desired clear value.
            IID_PPV_ARGS(&m_depthStencil)
            ));

        NAME_D3D12_OBJECT(m_depthStencil);

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvDescriptorHeap->GetCpuHandle(0));
    }


    // read back data
    {

        CD3DX12_HEAP_PROPERTIES ReadBackHeap(D3D12_HEAP_TYPE_READBACK);
        CD3DX12_RESOURCE_DESC  ReadBackResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(64);
        m_device->CreateCommittedResource(&ReadBackHeap, D3D12_HEAP_FLAG_NONE, &ReadBackResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&m_pReadBackResource));
    }

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_fenceValue = m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        m_commandQueue->WaitForFenceCPUBlocking(m_fenceValue);
    }

    CreateFrameResources();
}

// Update frame-based values.
void D3D12DynamicIndexing::OnUpdate()
{
    m_timer.Tick(NULL);

    if (m_frameCounter == 500)
    {
        // Update window text with FPS value.
        wchar_t fps[64];
        swprintf_s(fps, L"%ufps", m_timer.GetFramesPerSecond());
        SetCustomWindowText(fps);
        m_frameCounter = 0;
    }

    m_frameCounter++;

    // Get current GPU progress against submitted workload. Resources still scheduled 
    // for GPU execution cannot be modified or else undefined behavior will result.
    //const UINT64 lastCompletedFence = m_fence->GetCompletedValue();
    const UINT64 lastCompletedFence = m_commandQueue->GetLastCompletedFence();

    // Move to the next frame resource.
    m_currentFrameResourceIndex = (m_currentFrameResourceIndex + 1) % FrameCount;
    m_pCurrentFrameResource = m_frameResources[m_currentFrameResourceIndex];

    // Make sure that this frame resource isn't still in use by the GPU.
    m_commandQueue->WaitForFenceCPUBlocking(m_fenceValue);

    m_camera.Update(static_cast<float>(m_timer.GetElapsedSeconds()));
    m_pCurrentFrameResource->UpdateConstantBuffers(m_camera.GetViewMatrix(), m_camera.GetProjectionMatrix(0.8f, m_aspectRatio));
}

// Render the scene.
void D3D12DynamicIndexing::OnRender()
{
    PIXBeginEvent(m_commandQueue->Get(), 0, L"Render");

    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList(m_pCurrentFrameResource);

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_fenceValue = m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    PIXEndEvent(m_commandQueue->Get());

    // Present and update the frame index for the next frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Signal and increment the fence value.
    m_pCurrentFrameResource->m_fenceValue = m_fenceValue;
    //ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
   // m_fenceValue++;
}

void D3D12DynamicIndexing::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    {
       
        //const UINT64 fence = m_fenceValue;
        //const UINT64 lastCompletedFence = m_fence->GetCompletedValue();
        // Wait for the last submitted GPU workload.
        m_commandQueue->WaitForFenceCPUBlocking(m_fenceValue);

        // IMPORTANT: also wait for any GPU work queued by the last Present.
        // The fence signaled in ExecuteCommandLists() is inserted before Present().
        // If we exit right after Present, swapchain backbuffers can still be in-flight.
        const UINT64 presentFence = m_commandQueue->Signal();
        m_commandQueue->WaitForFenceCPUBlocking(presentFence);

        //// Signal and increment the fence value.
        //ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
        //m_fenceValue++;

        //// Wait until the previous frame is finished.
        //if (lastCompletedFence < fence)
        //{
        //    ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        //    WaitForSingleObject(m_fenceEvent, INFINITE);
        //}
        mQueueManager.reset();
    }

    for (UINT i = 0; i < m_frameResources.size(); i++)
    {
        delete m_frameResources.at(i);
    }
}

void D3D12DynamicIndexing::OnKeyDown(UINT8 key)
{
    m_camera.OnKeyDown(key);
}

void D3D12DynamicIndexing::OnKeyUp(UINT8 key)
{
    m_camera.OnKeyUp(key);
}

void D3D12DynamicIndexing::OnRButtonDown(int x, int y)
{
    m_camera.OnRButtonDown(x, y);
}

void D3D12DynamicIndexing::OnRButtonUp(int x, int y)
{
    m_camera.OnRButtonUp(x, y);
}

void D3D12DynamicIndexing::OnMouseMove(WPARAM btnState, int x, int y)
{
    (void)btnState;
    m_camera.OnMouseMove(x, y);
}


// Create the resources that will be used every frame.
void D3D12DynamicIndexing::CreateFrameResources()
{
    // Initialize each frame resource.
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(m_cbvSrvDescriptorHeap->GetCpuHandle(CityMaterialCount + 2));    // Move past the SRVs.
    for (UINT i = 0; i < FrameCount; i++)
    {
        FrameResource* pFrameResource = new FrameResource(m_device.Get(), CityRowCount, CityColumnCount, CityMaterialCount, CitySpacingInterval);

        UINT64 cbOffset = 0;
        for (UINT j = 0; j < CityRowCount; j++)
        {
            for (UINT k = 0; k < CityColumnCount; k++)
            {
                // Describe and create a constant buffer view (CBV).
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation = pFrameResource->m_cbvUploadHeap->GetGPUVirtualAddress() + cbOffset;
                cbvDesc.SizeInBytes = sizeof(FrameResource::SceneConstantBuffer);
                cbOffset += cbvDesc.SizeInBytes;
                m_device->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
                cbvSrvHandle.Offset(m_cbvSrvDescriptorSize);
            }
        }

        auto cityPsoObj = m_renderPipeline->GetGraphicsPipeline(L"CityGP");
        auto cityRootObj = m_renderPipeline->GetRootSignature(L"CityRS");
        auto* cityPso = dynamic_cast<FDX12RHIPipelineState*>(cityPsoObj.get());
        auto* cityRootSig = dynamic_cast<FDX12RHIRootSignature*>(cityRootObj.get());
        if (!cityPso || !cityRootSig)
        {
            ThrowIfFailed(E_FAIL);
        }

        pFrameResource->InitBundle(m_device.Get(), cityPso->GetNative(), i, m_numIndices, &m_indexBufferView,
            &m_vertexBufferView, m_cbvSrvDescriptorHeap->GetHeap(), m_cbvSrvDescriptorSize, m_samplerDescriptorHeap->GetHeap(), cityRootSig->GetNative());



        m_frameResources.push_back(pFrameResource);
    }
}

void D3D12DynamicIndexing::PopulateCommandList(FrameResource* pFrameResource)
{
    // Command list allocators can only be reset when the associated
    // command lists have finished execution on the GPU; apps should use
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_pCurrentFrameResource->m_commandAllocator->Reset());

    // Reset main command list.
    auto cityPsoObj = m_renderPipeline->GetGraphicsPipeline(L"CityGP");
    auto* cityPso = dynamic_cast<FDX12RHIPipelineState*>(cityPsoObj.get());
    ThrowIfFailed(m_commandList->Reset(m_pCurrentFrameResource->m_commandAllocator.Get(), cityPso ? cityPso->GetNative() : nullptr));

    // Wrap native DX12 objects into RHI-agnostic views and let the pipeline record passes.
    FDX12RHICommandList rhiCmd(m_commandList.Get());
    FDX12RHIDescriptorHeapView cbvHeap(m_cbvSrvDescriptorHeap->GetHeap());
    FDX12RHIDescriptorHeapView samplerHeap(m_samplerDescriptorHeap->GetHeap());

    FRenderPipeline::FRenderContext ctx;
    ctx.Cmd = &rhiCmd;

    ctx.BackBuffer = std::make_shared<FDX12RHITexture2D>(m_renderTargets[m_frameIndex].Get());
    ctx.BackBufferRTV = DX12_ToRHICpu(m_rtvDescriptorHeap->GetCpuHandle(m_frameIndex));

    ctx.DepthStencil = std::make_shared<FDX12RHITexture2D>(m_depthStencil.Get());
    ctx.DSV = DX12_ToRHICpu(m_dsvDescriptorHeap->GetCpuHandle(0));

    ctx.Viewport.TopLeftX = m_viewport.TopLeftX;
    ctx.Viewport.TopLeftY = m_viewport.TopLeftY;
    ctx.Viewport.Width = m_viewport.Width;
    ctx.Viewport.Height = m_viewport.Height;
    ctx.Viewport.MinDepth = m_viewport.MinDepth;
    ctx.Viewport.MaxDepth = m_viewport.MaxDepth;

    ctx.Scissor.left = m_scissorRect.left;
    ctx.Scissor.top = m_scissorRect.top;
    ctx.Scissor.right = m_scissorRect.right;
    ctx.Scissor.bottom = m_scissorRect.bottom;

    ctx.CbvSrvUavHeap = &cbvHeap;
    ctx.SamplerHeap = &samplerHeap;

    FDX12CityGeometryPass::FInputs cityInputs;
    cityInputs.Frame = pFrameResource;
    cityInputs.FrameResourceIndex = m_currentFrameResourceIndex;
    cityInputs.NumIndices = m_numIndices;
    cityInputs.IBV = &m_indexBufferView;
    cityInputs.VBV = &m_vertexBufferView;
    cityInputs.CbvSrvUavHeap = m_cbvSrvDescriptorHeap->GetHeap();
    cityInputs.SamplerHeap = m_samplerDescriptorHeap->GetHeap();
    cityInputs.CbvSrvUavDescriptorSize = m_cbvSrvDescriptorSize;
    cityInputs.bUseBundles = UseBundles;

    ctx.UserData = &cityInputs;

    m_renderPipeline->Execute(ctx);


}
