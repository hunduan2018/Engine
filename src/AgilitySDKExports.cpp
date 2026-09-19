// D3D12 Agility SDK entry points (https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/).
// The inbox D3D12.dll loader reads these exports to decide whether to load the
// app-local D3D12Core.dll instead of the System32 one.
// D3D12SDKVersion comes from the Agility SDK headers in ThirdParty/D3D12Agility
// and must match the minor version of the package (1.619.6 -> 619).
#include <d3d12.h>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }

extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }
