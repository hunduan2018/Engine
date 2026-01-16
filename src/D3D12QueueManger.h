#pragma once
#include "stdafx.h"
#include "Assert.h"
#include <mutex>

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class Direct3DQueue
{
public:
	Direct3DQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
	~Direct3DQueue();

	bool IsFenceComplete(uint64 fenceValue);
	void InsertWait(uint64 fenceValue);
	void InsertWaitForQueueFence(Direct3DQueue* otherQueue, uint64 fenceValue);
	void InsertWaitForQueue(Direct3DQueue* otherQueue);

	void WaitForFenceCPUBlocking(uint64 fenceValue);
	void WaitForIdle() { WaitForFenceCPUBlocking(mNextFenceValue - 1); }

	ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue.Get(); }
	ID3D12CommandQueue* Get() const { return mCommandQueue.Get(); }


	uint64 PollCurrentFenceValue();
	uint64 GetLastCompletedFence() { return mLastCompletedFenceValue; }
	uint64 GetNextFenceValue() { return mNextFenceValue; }
	ID3D12Fence* GetFence() { return mFence; }

	uint64 ExecuteCommandList(ID3D12CommandList* List);
	uint64 ExecuteCommandLists(UINT count, ID3D12CommandList* const* lists);

private:
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	D3D12_COMMAND_LIST_TYPE mQueueType;

	std::mutex mFenceMutex;
	std::mutex mEventMutex;

	ID3D12Fence* mFence;
	uint64 mNextFenceValue;
	uint64 mLastCompletedFenceValue;
	HANDLE mFenceEventHandle;
};

class Direct3DQueueManager
{
public:
	Direct3DQueueManager(ID3D12Device* device);
	~Direct3DQueueManager();

	Direct3DQueue* GetGraphicsQueue() { return mGraphicsQueue; }
	Direct3DQueue* GetComputeQueue() { return mComputeQueue; }
	Direct3DQueue* GetCopyQueue() { return mCopyQueue; }

	Direct3DQueue* GetQueue(D3D12_COMMAND_LIST_TYPE commandType);

	bool IsFenceComplete(uint64 fenceValue);
	void WaitForFenceCPUBlocking(uint64 fenceValue);
	void WaitForAllIdle();

private:
	Direct3DQueue* mGraphicsQueue;
	Direct3DQueue* mComputeQueue;
	Direct3DQueue* mCopyQueue;
};