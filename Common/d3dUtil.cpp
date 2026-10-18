#include "../src/stdafx.h"
#include "d3dUtil.h"
#include "d3dx12.h"

#include <fstream>
#include <sstream>

DxException::DxException(
    HRESULT hr,
    const std::string& functionName,
    const std::string& filename,
    int lineNumber)
    : std::runtime_error(functionName),
      m_errorCode(hr),
      m_functionName(functionName),
      m_filename(filename),
      m_lineNumber(lineNumber)
{
}

std::string DxException::ToString() const
{
    char messageBuffer[512] = {};
    DWORD length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(m_errorCode),
        0,
        messageBuffer,
        static_cast<DWORD>(sizeof(messageBuffer)),
        nullptr);

    std::string messageText = length > 0
        ? std::string(messageBuffer, length)
        : "Unknown HRESULT";

    std::ostringstream result;
    result << m_functionName << " failed in " << m_filename << "; line "
        << m_lineNumber << "; error: " << messageText;
    return result.str();
}

namespace d3dUtil
{
    void ThrowIfFailed(HRESULT hr, const char* functionName, const char* filename, int lineNumber)
    {
        if (FAILED(hr))
        {
            throw DxException(hr, functionName, filename, lineNumber);
        }
    }

    bool IsKeyDown(int vkeyCode)
    {
        return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename)
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file)
        {
            throw std::runtime_error("Failed to open binary file.");
        }

        std::streamsize size = file.tellg();
        if (size < 0)
        {
            throw std::runtime_error("Failed to determine binary file size.");
        }

        file.seekg(0, std::ios::beg);

        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        D3D12_THROW_IF_FAILED(D3DCreateBlob(static_cast<SIZE_T>(size), &blob));

        if (size > 0 && !file.read(static_cast<char*>(blob->GetBufferPointer()), size))
        {
            throw std::runtime_error("Failed to read binary file.");
        }

        return blob;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> LoadShaderBytecode(const std::wstring& filename)
    {
        return LoadBinary(filename);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

        CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
        D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&defaultBuffer)));

        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_THROW_IF_FAILED(device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)));

        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = initData;
        subResourceData.RowPitch = static_cast<LONG_PTR>(byteSize);
        subResourceData.SlicePitch = static_cast<LONG_PTR>(byteSize);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST));
        UpdateSubresources<1>(commandList, defaultBuffer.Get(), uploadBuffer.Get(),
            0, 0, 1, &subResourceData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ));

        return defaultBuffer;
    }
}
