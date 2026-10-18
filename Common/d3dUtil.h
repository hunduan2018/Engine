#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

#include <stdexcept>
#include <string>

class DxException : public std::runtime_error
{
public:
	DxException() = default;
    DxException(HRESULT hr, const std::string& functionName, const std::string& filename, int lineNumber);

    HRESULT ErrorCode() const noexcept { return m_errorCode; }
    std::string ToString() const;

private:
    HRESULT m_errorCode = S_OK;
    std::string m_functionName;
    std::string m_filename;
    int m_lineNumber = -1;
};

namespace d3dUtil
{
    void ThrowIfFailed(HRESULT hr, const char* functionName, const char* filename, int lineNumber);

    inline UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        return (byteSize + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
            ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    }

    bool IsKeyDown(int vkeyCode);

    Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    // Loads DXC-generated DXIL bytecode from a .cso file.
    Microsoft::WRL::ComPtr<ID3DBlob> LoadShaderBytecode(const std::wstring& filename);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
}

#define D3D12_THROW_IF_FAILED(x) \
    ::d3dUtil::ThrowIfFailed((x), #x, __FILE__, __LINE__)
