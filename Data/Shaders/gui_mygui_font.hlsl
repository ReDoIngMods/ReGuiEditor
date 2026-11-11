// Shader purely exists just to convert Luminance to RGBA8. Otherwise the font glyphs would
// be rendered from black to red without alpha.

struct VertexOutput {
    float4 position : SV_POSITION;
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

#if defined(VERTEX_SHADER)

struct VertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VertexOutput vsMain(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    output.uv = input.uv;

    return output;
}

#endif

#if defined(PIXEL_SHADER)

Texture2D fontTexture : register(t0);
SamplerState fontSampler : register(s0);

float4 psMain(VertexOutput input) : SV_TARGET {
    float alpha = fontTexture.Sample(fontSampler, input.uv).r;
    return float4(1.0f, 1.0f, 1.0f, alpha) * input.color;
}

#endif