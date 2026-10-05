struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

#if defined(VERTEX_SHADER)

cbuffer ConstantBuffer : register(b0) {
    float2 displayPosition;
    float2 displayResolution;
};

struct VertexInput {
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VertexOutput vsMain(VertexInput input) {
    float2 normalizedPosition = ((input.position - displayPosition) / displayResolution) * 2.0 - 1.0;
    normalizedPosition.y *= -1.0;

    VertexOutput output;
    output.position = float4(normalizedPosition, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

#endif

#if defined(PIXEL_SHADER)

Texture2D<float4> sampleTexture : register(t0);
SamplerState sampleSampler : register(s0);

float4 psMain(VertexOutput input) : SV_TARGET {
    return input.color * sampleTexture.Sample(sampleSampler, input.uv);
}

#endif