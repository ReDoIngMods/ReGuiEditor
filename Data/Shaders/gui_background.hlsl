#if defined(PIXEL_SHADER)

#include "gui_fullscreen_triangle.hlsl"

Texture2D inputTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0) {
    float2 uvScale;
};

float4 psMain(VertexOutput input) : SV_TARGET {
    return inputTexture.Sample(samplerState, input.uv * uvScale);
}

#endif