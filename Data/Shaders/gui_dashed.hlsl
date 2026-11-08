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

// ---- Properties for the dashed line shader ----
static const float DASH_LENGTH   = 8.0;		// Length of the visible dash segment
static const float GAP_LENGTH    = 6.0;		// Length of the gap between dashes
static const float BORDER_WIDTH  = 1.0;		// Width of the border around the dashed line
static const float FILL_ALPHA    = 0.2;		// Alpha value for the fill area of the dashed line

float4 psMain(VertexOutput input) : SV_TARGET {
	// Calculate the UV coordinates per pixel to determine the size and position of the dashed line segments.
	float2 uvPerPixel = max(float2(abs(ddx(input.uv.x)), abs(ddy(input.uv.y))), 1e-6);

	// Calculate the size and position of the dashed line segments based on the UV coordinates and the UV per pixel.
	float2 size = 1.0 / uvPerPixel;
	float2 position = input.uv * size;

	// Calculate the distance to the nearest edge of the dashed line segment.
	float2 edgeDistance = min(position, size - position);

	// Determine whether the pixel is on a horizontal or vertical edge based on the edge distances.
	bool horizontalEdge = edgeDistance.y <= edgeDistance.x;
	float along = horizontalEdge ? position.x : position.y;

	// Determine whether the pixel is on the border of the dashed line and whether it is part of a dash segment.
	bool onBorder = min(edgeDistance.x, edgeDistance.y) < BORDER_WIDTH;
	bool onDash = fmod(along, DASH_LENGTH + GAP_LENGTH) < DASH_LENGTH;

	// Set the alpha value based on whether the pixel is on a border and part of a dash segment, or if it is in the fill area.
	float alpha = (onBorder && onDash) ? 1.0 : FILL_ALPHA;
	return float4(input.color.rgb, input.color.a * alpha);
}

#endif