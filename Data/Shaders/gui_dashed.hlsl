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

static const float DASH_LENGTH   = 8.0;
static const float GAP_LENGTH    = 6.0;
static const float BORDER_WIDTH  = 1.0;
static const float FILL_ALPHA    = 0.2;

float4 psMain(VertexOutput input) : SV_TARGET {
	float2 uvPerPixel = max(float2(abs(ddx(input.uv.x)), abs(ddy(input.uv.y))), 1e-6);

	float2 size = 1.0 / uvPerPixel;
	float2 position = input.uv * size;

	float2 edgeDistance = min(position, size - position);

	bool horizontalEdge = edgeDistance.y <= edgeDistance.x;
	float along = horizontalEdge ? position.x : position.y;

	bool onBorder = min(edgeDistance.x, edgeDistance.y) < BORDER_WIDTH;
	bool onDash = fmod(along, DASH_LENGTH + GAP_LENGTH) < DASH_LENGTH;

	float alpha = (onBorder && onDash) ? 1.0 : FILL_ALPHA;
	return float4(input.color.rgb, input.color.a * alpha);
}

#endif
