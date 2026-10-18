// FShaderManager.h
// 着色器字典：按名加载 exe 目录下 Shaders/*.cso，句柄即数组索引。
// 预留的 AS/MS 阶段在第二阶段只是多一个名字，无需改动本类。

#pragma once

#include "RendererTypes.h"

#include <d3d12.h>
#include <wrl.h>

#include <string>
#include <vector>

class FShaderManager
{
public:
    // 按名加载或取回已缓存的着色器（name 不含路径与 .cso 后缀，如 "shader_basepass_vert"）。
    // 文件位于 <exe 目录>/Shaders/<name>.cso。文件缺失抛 DxException。
    FShaderHandle LoadShader(const std::string& name);

    // 取 DXIL 字节码；句柄无效时断言。
    D3D12_SHADER_BYTECODE GetShaderBytecode(FShaderHandle handle) const;

    uint32_t GetShaderCount() const { return static_cast<uint32_t>(ShaderBlobs.size()); }

private:
    std::vector<Microsoft::WRL::ComPtr<ID3DBlob>> ShaderBlobs;
    std::vector<std::string> ShaderNames;
};
