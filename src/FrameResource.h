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

#pragma once

#include "DXSampleHelper.h"
#include <windows.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class FrameResource
{
private:
    void SetCityPositions(FLOAT intervalX, FLOAT intervalZ);

public:
    struct SceneConstantBuffer
    {
        XMFLOAT4X4 mvp;        // Model-view-projection (MVP) matrix.
        FLOAT padding[48];
    };

    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_GraphicCommandList;
    ComPtr<ID3D12Resource> m_cbvUploadHeap;
    SceneConstantBuffer* m_pConstantBuffers;
    UINT64 m_fenceValue;

    std::vector<XMFLOAT4X4> m_modelMatrices;
    UINT m_cityRowCount;
    UINT m_cityColumnCount;
    UINT m_cityMaterialCount;
    UINT m_StructureBufferCount;
    std::vector<UINT> m_StructBufferSize;
    ID3D12Device* m_pDevice;

    FrameResource(ID3D12Device* pDevice, UINT cityRowCount, UINT cityColumnCount, UINT cityMaterialCount, float citySpacingInterval);
    ~FrameResource();

    void InitBundle(ID3D12Device* pDevice, ID3D12PipelineState* pPso,
        UINT frameResourceIndex, UINT numIndices, D3D12_INDEX_BUFFER_VIEW* pIndexBufferViewDesc, D3D12_VERTEX_BUFFER_VIEW* pVertexBufferViewDesc,
        ID3D12DescriptorHeap* pCbvSrvDescriptorHeap, UINT cbvSrvDescriptorSize, ID3D12DescriptorHeap* pSamplerDescriptorHeap, ID3D12RootSignature* pRootSignature);

    void PopulateCommandList(ID3D12GraphicsCommandList* pCommandList,
        UINT frameResourceIndex, UINT numIndices, D3D12_INDEX_BUFFER_VIEW* pIndexBufferViewDesc, D3D12_VERTEX_BUFFER_VIEW* pVertexBufferViewDesc,
        ID3D12DescriptorHeap* pCbvSrvDescriptorHeap, UINT cbvSrvDescriptorSize, ID3D12DescriptorHeap* pSamplerDescriptorHeap, ID3D12RootSignature* pRootSignature);

    void XM_CALLCONV UpdateConstantBuffers(FXMMATRIX view, CXMMATRIX projection);


    // Direct3D 12: One buffer to accommodate different types of resources
	ComPtr<ID3D12Resource> m_spUploadBuffer;
	UINT8* m_pDataBegin = nullptr;    // starting position of upload buffer
	UINT8* m_pDataCur = nullptr;      // current position of upload buffer
	UINT8* m_pDataEnd = nullptr;      // ending position of upload buffer

	//
    // Create an upload buffer and keep it always mapped.
    //

	HRESULT InitializeUploadBuffer(SIZE_T uSize)
	{
		HRESULT hr = m_pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uSize),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&m_spUploadBuffer));

		if (SUCCEEDED(hr))
		{
			void* pData;
			//
			// No CPU reads will be done from the resource.
			//
			CD3DX12_RANGE readRange(0, 0);
			m_spUploadBuffer->Map(0, &readRange, &pData);
			m_pDataCur = m_pDataBegin = reinterpret_cast<UINT8*>(pData);
			m_pDataEnd = m_pDataBegin + uSize;
		}
		return hr;
	}

	//
	// Sub-allocate from the buffer, with offset aligned.
	//

	HRESULT SuballocateFromBuffer(SIZE_T uSize, UINT uAlign)
	{
		m_pDataCur = reinterpret_cast<UINT8*>(
			Align(reinterpret_cast<SIZE_T>(m_pDataCur), uAlign)
			);

		return (m_pDataCur + uSize > m_pDataEnd) ? E_INVALIDARG : S_OK;
	}

	//
	// Place and copy data to the upload buffer.
	//

	HRESULT SetDataToUploadBuffer(
		const void* pData,
		UINT bytesPerData,
		UINT dataCount,
		UINT alignment,
		UINT& byteOffset
	)
	{
		SIZE_T byteSize = bytesPerData * dataCount;
		HRESULT hr = SuballocateFromBuffer(byteSize, alignment);
		if (SUCCEEDED(hr))
		{
			byteOffset = UINT(m_pDataCur - m_pDataBegin);
			memcpy(m_pDataCur, pData, byteSize);
			m_pDataCur += byteSize;
		}
		return hr;
	}

	//
	// Align uLocation to the next multiple of uAlign.
	//

	UINT Align(UINT uLocation, UINT uAlign)
	{
		if ((0 == uAlign) || (uAlign & (uAlign - 1)))
		{
			//ThrowException("non-pow2 alignment");
			//throw();
			return -1;
		}

		return ((uLocation + (uAlign - 1)) & ~(uAlign - 1));
	}
};
