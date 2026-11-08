#if defined(PIXEL_SHADER)

#include "gui_fullscreen_triangle.hlsl"

Texture2D inputTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer Constants : register(b0) {
    float2 screenResolution;
    float2 imageResolution;
}

float4 psMain(VertexOutput input) : SV_TARGET {
    float screenAspect = screenResolution.x / screenResolution.y;
    float imageAspect = imageResolution.x / imageResolution.y;

    // Fraction of the screen the image occupies on each axis
    float2 uvScale = float2(1.0, 1.0);
    if (screenAspect > imageAspect) {
        uvScale.x = imageAspect / screenAspect; // screen is wider: bars left/right
    } else {
        uvScale.y = screenAspect / imageAspect; // screen is taller: bars top/bottom
    }

    // Center the image and map screen UVs to texture UVs
    float2 uv = (input.uv - 0.5) / uvScale + 0.5;
    if (any(uv < 0.0) || any(uv > 1.0)) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    return inputTexture.Sample(samplerState, uv);
}

#endif