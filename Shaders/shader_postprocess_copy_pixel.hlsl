struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D g_Source : register(t0, space0);
SamplerState g_Sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return g_Source.Sample(g_Sampler, input.uv);
}
