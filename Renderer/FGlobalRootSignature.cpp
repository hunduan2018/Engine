// FGlobalRootSignature.cpp

#include "FGlobalRootSignature.h"

#include "RendererTypes.h"
#include "d3dUtil.h"
#include "d3dx12.h"

void FGlobalRootSignature::Initialize(ID3D12Device* device)
{
    CD3DX12_ROOT_PARAMETER1 slotRootParameters[RootParam::Count] = {};

    // [0] b0 视图常量 CBV；[1] b1 材质常量 CBV。曲面细分的 HS/DS 同样从 b0/b1 读取，
    // 因此全部参数使用 SHADER_VISIBILITY_ALL（InitAs* 的默认值）。
    // DATA_STATIC：常量在设置根 CBV 前已写入上传环，执行期间同帧槽不被重写
    //（fence 等待保证）——与 UE5 相同的声明（D3D12RootSignature.cpp:150）。
    constexpr D3D12_ROOT_DESCRIPTOR_FLAGS CbvDataStatic = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
    slotRootParameters[RootParam::ViewCBV].InitAsConstantBufferView(0, 0, CbvDataStatic);
    slotRootParameters[RootParam::MaterialCBV].InitAsConstantBufferView(1, 0, CbvDataStatic);
    slotRootParameters[RootParam::DrawConstants].InitAsConstants(4, 2, 0, D3D12_SHADER_VISIBILITY_ALL);

    // [3] bindless SRV 表：t0 起连续 BindlessSrvCount 个。堆槽 0 = 实例缓冲，1.. = 纹理。
    // 描述符本身初始化后不变，但**数据**每帧更新（实例缓冲默认堆 copy）；
    // 本根签名全引擎唯一，SRV 表只在首次设置根签名时绑定、跨帧持续存在，
    // 因此不能声明任何 DATA_STATIC* ——那承诺"表设置后数据不变"，会被每帧的
    // 实例写入违反，触发未定义行为（实测：调试层下 root constants 读值错乱）。
    // DESCRIPTORS_VOLATILE 允许描述符晚绑定，对数据无约束，语义安全。
    CD3DX12_DESCRIPTOR_RANGE1 srvRange = {};
    srvRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        RendererLimits::BindlessSrvCount,
        0,      // base shader register: t0
        0,      // register space 0
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    slotRootParameters[RootParam::BindlessSrvTable].InitAsDescriptorTable(
        1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);

    // 静态采样器：s0 线性/Wrap/各向异性 16；s1 点/Clamp。
    CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
    staticSamplers[0].Init(
        0, D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, 16);
    staticSamplers[1].Init(
        1, D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    // 项目强制使用 D3D12 Agility SDK 运行时，根签名 1.1 始终可用，无需 1.0 回退。
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Init_1_1(
        _countof(slotRootParameters), slotRootParameters,
        _countof(staticSamplers), staticSamplers,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    D3D12_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));

    D3D12_THROW_IF_FAILED(device->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&RootSignature)));
    RootSignature->SetName(L"FGlobalRootSignature");
}
