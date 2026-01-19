
#pragma once	
#include <d3d12.h>
#include "Direct3DUtils.h"
#include "RHIResource.h"

class DX12RHIResource : public RHIResource
{
public:
	DX12RHIResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState,ERHIResourceType Type);
	virtual ~DX12RHIResource();

	ID3D12Resource* GetResource() { return mResource; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() { return mGPUAddress; }
	D3D12_RESOURCE_STATES GetUsageState() { return mUsageState; }
	void SetUsageState(D3D12_RESOURCE_STATES usageState) { mUsageState = usageState; }

	bool GetIsReady() { return mIsReady; }
	void SetIsReady(bool isReady) { mIsReady = isReady; }

protected:
	ID3D12Resource* mResource;
	D3D12_GPU_VIRTUAL_ADDRESS mGPUAddress;
	D3D12_RESOURCE_STATES mUsageState;
	bool mIsReady;
};

DX12RHIResource::DX12RHIResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, ERHIResourceType Type):RHIResource(Type)
{
	mResource = resource;
	mUsageState = usageState;
	mGPUAddress = 0;
	mIsReady = false;
}

DX12RHIResource::~DX12RHIResource()
{
	mResource->Release();
	mResource = NULL;
}