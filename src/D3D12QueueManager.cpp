#include "stdafx.h"
#include "D3D12QueueManger.h"
#include "DXSampleHelper.h"
#include "MathHelper.h"

Direct3DQueue::Direct3DQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType)
{
	mQueueType = commandType;
	mCommandQueue = NULL;
	mFence = NULL;
	mNextFenceValue = ((uint64_t)mQueueType << 56) + 1;
	mLastCompletedFenceValue = ((uint64_t)mQueueType << 56);

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = mQueueType;
	queueDesc.NodeMask = 0;
	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));
	
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	mFence->Signal(mLastCompletedFenceValue);

	mFenceEventHandle = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	APP_CHECK(mFenceEventHandle != INVALID_HANDLE_VALUE);
}

Direct3DQueue::~Direct3DQueue()
{
	CloseHandle(mFenceEventHandle);

	mFence->Release();
	mFence = NULL;

	mCommandQueue->Release();
	mCommandQueue = NULL;
}

uint64 Direct3DQueue::PollCurrentFenceValue()
{
	mLastCompletedFenceValue = MathHelper::Max(mLastCompletedFenceValue, mFence->GetCompletedValue());
	return mLastCompletedFenceValue;
}

bool Direct3DQueue::IsFenceComplete(uint64 fenceValue)
{
	if (fenceValue > mLastCompletedFenceValue)
	{
		PollCurrentFenceValue();
	}

	return fenceValue <= mLastCompletedFenceValue;
}