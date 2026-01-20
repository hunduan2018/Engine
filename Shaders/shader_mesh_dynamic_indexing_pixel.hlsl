//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
    float4 position    : SV_POSITION;
    float2 uv        : TEXCOORD0;
};

struct  ConstData
{
    float4 bar;
};

struct MaterialConstants
{
    uint matIndex;    // Dynamically set index for looking up from g_txMats[].
    uint2 bar;
    uint moo; // Structure buffer Size
};

StructuredBuffer<ConstData> g_ConstData[]: register(t0, space1);
//ByteAddressBuffer g_AdBuffer[] : register(t3, space0);

ConstantBuffer<MaterialConstants> materialConstants : register(b0, space0);
//Texture2D        g_txDiffuse    : register(t0,space0);
Texture2D        g_txMats[]    : register(t1,space0);
SamplerState    g_sampler[]    : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 diffuse = g_txMats[0].Sample(g_sampler[0], input.uv).rgb;
    float3 mat = g_txMats[NonUniformResourceIndex(materialConstants.matIndex+1)].Sample(g_sampler[0], input.uv).rgb;
    uint BufferIndex = materialConstants.matIndex - (materialConstants.matIndex/4) * 4;
    const StructuredBuffer<ConstData> StrBuffer = g_ConstData[NonUniformResourceIndex(BufferIndex)];
    ConstData Data = StrBuffer[0];
    float x = Data.bar.y;
    return float4(diffuse * mat*x/255.0f, 1.0f);
}
