#if defined(PIXEL_SHADER)

cbuffer GridConstants : register(b0) {
	float2 gridOrigin;			// The origin point of the grid in screen space coordinates
	float2 gridPan;				// The pan offset of the grid in screen space coordinates
	float gridCellSize;			// The size of each grid cell in screen space coordinates
	float3 gridPadding;			// Padding to align the constant buffer to a 16-byte boundary
	
	float4 backgroundColor;		// The color of the background of the grid
	float4 checkerColor;		// The color of the checker pattern of the grid
};

float4 psMain(float4 position : SV_Position) : SV_Target {
	// Calculate the position of the pixel in grid space by applying the grid origin, pan, and cell size.
	float2 gridPosition = (position.xy - gridOrigin + gridPan) / gridCellSize;
	int2 cellPosition = (int2)floor(gridPosition);

	// Return the color of the pixel based on whether it is in a checker pattern or in the background color.
	if (((cellPosition.x ^ cellPosition.y) & 1) != 0) {
		return checkerColor;
	} else {
		return backgroundColor;
	}
}

#endif