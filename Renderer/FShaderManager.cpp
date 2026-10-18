// FShaderManager.cpp

#include "FShaderManager.h"

#include "../src/Assert.h"
#include "d3dUtil.h"

#include <cstdlib>

// 取 exe 所在目录（着色器与资产均按 exe 相对路径解析，工作目录已由 CMake 固定为 exe 目录）。
static std::wstring GetExecutableDir()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t drive[_MAX_DRIVE] = {};
    wchar_t dir[_MAX_DIR] = {};
    _wsplitpath_s(exePath, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
    return std::wstring(drive) + dir;
}

FShaderHandle FShaderManager::LoadShader(const std::string& name)
{
    // 已加载过则直接返回缓存句柄。
    for (uint32_t index = 0; index < ShaderNames.size(); ++index)
    {
        if (ShaderNames[index] == name)
        {
            return index;
        }
    }

    const std::wstring path = GetExecutableDir() + L"Shaders\\" + std::wstring(name.begin(), name.end()) + L".cso";
    Microsoft::WRL::ComPtr<ID3DBlob> blob = d3dUtil::LoadShaderBytecode(path);

    ShaderBlobs.push_back(std::move(blob));
    ShaderNames.push_back(name);
    return static_cast<FShaderHandle>(ShaderBlobs.size() - 1);
}

D3D12_SHADER_BYTECODE FShaderManager::GetShaderBytecode(FShaderHandle handle) const
{
    APP_CHECK(handle < ShaderBlobs.size());
    D3D12_SHADER_BYTECODE bytecode = {};
    bytecode.pShaderBytecode = ShaderBlobs[handle]->GetBufferPointer();
    bytecode.BytecodeLength = ShaderBlobs[handle]->GetBufferSize();
    return bytecode;
}
