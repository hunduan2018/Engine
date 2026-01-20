#pragma once

#include <DirectXMath.h>

// Simple aliases for DirectXMath types.
// Keep these in a shared header to avoid including unrelated modules just for type names.
typedef DirectX::XMFLOAT2  FVector2;
typedef DirectX::XMFLOAT3  FVector3;
typedef DirectX::XMFLOAT4  FVector4;
typedef DirectX::XMFLOAT4X4 FMatrix4x4;

// SIMD math types (register types).
typedef DirectX::XMVECTOR  FSimdVector;
typedef DirectX::XMMATRIX  FSimdMatrix;



