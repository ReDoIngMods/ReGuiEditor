#if defined(PIXEL_SHADER)

#include "gui_fullscreen_triangle.hlsl"

Texture2D backdropTexture : register(t0);
Texture2D atlasTexture : register(t1);
SamplerState samplerState : register(s0);

cbuffer ConstantBufferData : register(b0) {
	float4 backdropUV;
	float4 atlasUV;
};

// This function performs a linear interpolation between two colors, but it ensures that the alpha value of
// the result is always 1.0 (fully opaque). Prevents you from seeing the backdrop through the atlas texture
// when the atlas texture is partially transparent.
float4 lerpFixedAlpha(float4 a, float4 b, float t) {
	float4 result = lerp(a, b, t);
	result.a = 1.0;

	return result;
}

float4 psMain(VertexOutput input) : SV_TARGET {
	// Calculate the UV coordinates for sampling the backdrop and atlas textures based on the input UVs.
	float2 atlasUVCoordinates = lerp(atlasUV.xy, atlasUV.zw, input.uv);
	float2 backdropUVCoordinates = lerp(backdropUV.xy, backdropUV.zw, input.uv);
	
	// Sample the colors from the backdrop and atlas textures using the calculated UV coordinates.
	float4 backdropColor = backdropTexture.Sample(samplerState, backdropUVCoordinates);
	float4 atlasColor = atlasTexture.Sample(samplerState, atlasUVCoordinates);

	// If the alpha value of the atlas color is less than 0.03, return the backdrop color directly.
	float alpha = atlasColor.a;
	if (alpha < 0.03)
		return atlasColor;

	// Perform a linear interpolation between the backdrop color and the atlas color
	return lerpFixedAlpha(backdropColor, atlasColor, alpha);
}

#endif