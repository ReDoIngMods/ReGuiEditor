#if defined(PIXEL_SHADER)

#include "gui_fullscreen_triangle.hlsl"

Texture2D backdropTexture : register(t0);
Texture2D atlasTexture : register(t1);
SamplerState samplerState : register(s0);

cbuffer ConstantBufferData : register(b0) {
	float4 backdropUV;
	float4 atlasUV;
};

float4 lerpFixedAlpha(float4 a, float4 b, float t) {
	float4 result = lerp(a, b, t);
	result.a = 1.0;

	return result;
}

float4 psMain(VertexOutput input) : SV_TARGET {
	float2 atlasUVCoordinates = lerp(atlasUV.xy, atlasUV.zw, input.uv);
	float2 backdropUVCoordinates = lerp(backdropUV.xy, backdropUV.zw, input.uv);
	
	float4 backdropColor = backdropTexture.Sample(samplerState, backdropUVCoordinates);
	float4 atlasColor = atlasTexture.Sample(samplerState, atlasUVCoordinates);

	float alpha = atlasColor.a;
	if (alpha < 0.03)
		return atlasColor;

	return lerpFixedAlpha(backdropColor, atlasColor, alpha);
}

#endif