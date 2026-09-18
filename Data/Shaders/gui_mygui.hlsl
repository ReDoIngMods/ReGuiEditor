struct VertexOutput {
    float4 position : SV_POSITION;
    float4 color : TEXCOORD0;
#if defined(TEXTURED)
    float2 uv : TEXCOORD1;
#endif
};

#if defined(VERTEX_SHADER)

struct VertexInput {
    float3 position : POSITION0;
    float4 color : COLOR0;
#if defined(TEXTURED)
    float2 uv : TEXCOORD0;
#endif
};

VertexOutput vsMain(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;

#if defined(TEXTURED)
    output.uv = input.uv;
#endif

    return output;
}

#endif

#if defined(PIXEL_SHADER)

#if defined(TEXTURED)
Texture2D<float4> sampleTexture : register(t0);
SamplerState sampleSampler : register(s0);

cbuffer ConstantBuffer : register(b0) {
    float2 uvScale;
};

#endif

float4 psMain(VertexOutput input) : SV_TARGET {
    float4 color = input.color;

#if defined(TEXTURED)
    color *= sampleTexture.Sample(sampleSampler, input.uv);
#endif

    return color;
}

#endif