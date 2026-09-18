#if defined(PIXEL_SHADER)

#include "gui_fullscreen_triangle.hlsl"

Texture2D sourceTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0) {
    float2 texelSize;
};

static const float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

float4 psMainHorizontal(VertexOutput input) : SV_TARGET {
    float4 result = sourceTexture.Sample(samplerState, input.uv) * weights[0];

    for (int index = 1; index < 5; ++index) {
        float2 offset = float2(texelSize.x * index, 0.0);

        result += sourceTexture.Sample(samplerState, input.uv + offset) * weights[index];
        result += sourceTexture.Sample(samplerState, input.uv - offset) * weights[index];
    }

    return result;
}

float4 psMainVertical(VertexOutput input) : SV_TARGET {
    float4 result = sourceTexture.Sample(samplerState, input.uv) * weights[0];

    for (int index = 1; index < 5; ++index) {
        float2 offset = float2(0.0, texelSize.y * index);

        result += sourceTexture.Sample(samplerState, input.uv + offset) * weights[index];
        result += sourceTexture.Sample(samplerState, input.uv - offset) * weights[index];
    }

    return result;
}

float4 psMainCopy(VertexOutput input) : SV_TARGET {
    return sourceTexture.Sample(samplerState, input.uv);
}

#endif