#if defined(PIXEL_SHADER)

cbuffer GridConstants : register(b0) {
	float2 gridOrigin;
	float2 gridPan;
	float gridCellSize;
	float3 gridPadding;
	
	float4 backgroundColor;
	float4 checkerColor;
};

float4 psMain(float4 position : SV_Position) : SV_Target {
	float2 gridPosition = (position.xy - gridOrigin + gridPan) / gridCellSize;
	int2 cellPosition = (int2)floor(gridPosition);

	if (((cellPosition.x ^ cellPosition.y) & 1) != 0) {
		return checkerColor;
	} else {
		return backgroundColor;
	}
}

#endif