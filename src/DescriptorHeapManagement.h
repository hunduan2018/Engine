#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <stdexcept>
#include "d3dx12.h" // 微软官方辅助库

using Microsoft::WRL::ComPtr;

// 基础堆封装
class DescriptorHeap {
public:
	DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool isShaderVisible,UINT NodeMask = 0u)
		: m_Type(type), m_MaxDescriptors(numDescriptors) {

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type;
		desc.NumDescriptors = numDescriptors;
		desc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = NodeMask;

		if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)))) {
			throw std::runtime_error("Failed to create descriptor heap");
		}

		m_DescriptorSize = device->GetDescriptorHandleIncrementSize(type);
		m_CPUStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
		m_GPUStart = isShaderVisible ? m_Heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	}

	ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(UINT index) const {
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CPUStart, index, m_DescriptorSize);
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(UINT index) const {
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GPUStart, index, m_DescriptorSize);
	}

	UINT GetDescriptorSize() const { return m_DescriptorSize; }

	void SetName(const std::wstring& name) {
		m_Heap->SetName(name.c_str());
	}
private:
	ComPtr<ID3D12DescriptorHeap> m_Heap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUStart;
	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUStart;
	UINT m_DescriptorSize;
	UINT m_MaxDescriptors;
	D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
};

// 适用于：SRV (纹理), UAV, 静态 CBV
// 策略：Pre-filled / Huge Array
class BindlessAllocator {
public:
	BindlessAllocator(ID3D12Device* device, UINT maxDescriptors)
		: m_Heap(std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, maxDescriptors, true))
	{
		// 初始化空闲列表，所有槽位初始都是空的
		for (UINT i = 0; i < maxDescriptors; ++i) {
			m_FreeIndices.push(i);
		}
	}

	// 分配一个固定的槽位
	// 返回值：Index (用于 Shader 中的索引)
	UINT Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle) {
		std::lock_guard<std::mutex> lock(m_AllocationMutex);
		if (m_FreeIndices.empty()) {
			throw std::runtime_error("Bindless Heap out of memory");
		}

		UINT index = m_FreeIndices.front();
		m_FreeIndices.pop();

		outCpuHandle = m_Heap->GetCpuHandle(index);
		return index;
	}

	// 释放槽位
	void Free(UINT index) {
		std::lock_guard<std::mutex> lock(m_AllocationMutex);
		m_FreeIndices.push(index);
	}

	ID3D12DescriptorHeap* GetHeap() const { return m_Heap->GetHeap(); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFirstGpuHandle() const { return m_Heap->GetGpuHandle(0); }

private:
	std::unique_ptr<DescriptorHeap> m_Heap;
	std::queue<UINT> m_FreeIndices;
	std::mutex m_AllocationMutex;
};

// 适用于：每帧变化的 CBV (如 Object Transform), 动态 SRV (如 UI, 粒子)
// 策略：Basic Strategy (Linear Allocation)
class LinearAllocator {
public:
	LinearAllocator(ID3D12Device* device, UINT maxDescriptors)
		: m_Heap(std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, maxDescriptors, true)),
		m_CurrentOffset(0), m_MaxDescriptors(maxDescriptors)
	{
	}

	// 每帧开始时调用，重置指针
	void Reset() {
		m_CurrentOffset = 0;
	}

	// 分配一块连续的描述符区域
	// 返回：GPU Handle (用于 SetGraphicsRootDescriptorTable)
	D3D12_GPU_DESCRIPTOR_HANDLE Allocate(UINT numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle) {
		if (m_CurrentOffset + numDescriptors > m_MaxDescriptors) {
			throw std::runtime_error("Linear Heap out of memory (increase size or optimize)");
		}

		UINT startIndex = m_CurrentOffset;
		outCpuHandle = m_Heap->GetCpuHandle(startIndex);
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_Heap->GetGpuHandle(startIndex);

		m_CurrentOffset += numDescriptors;
		return gpuHandle;
	}

	ID3D12DescriptorHeap* GetHeap() const { return m_Heap->GetHeap(); }

private:
	std::unique_ptr<DescriptorHeap> m_Heap;
	UINT m_CurrentOffset;
	UINT m_MaxDescriptors;
};