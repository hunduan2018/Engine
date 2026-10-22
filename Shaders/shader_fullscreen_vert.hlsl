struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vertexId : SV_VertexID)
{
    // Fullscreen triangle (no vertex buffer).
    // Positions cover the viewport after rasterization.
    const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f),
    };

    const float2 uvs[3] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f),
    };

    VSOut o;
    o.position = float4(positions[vertexId], 0.0f, 1.0f);
    o.uv = uvs[vertexId];
    return o;
}
