struct VertexOutput {
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD1;
};

#if defined(VERTEX_SHADER)

VertexOutput vsMain(uint vertexID : SV_VertexID) {
	VertexOutput output;

	float2 position = float2((vertexID == 2) ? 3.0 : -1.0, (vertexID == 1) ? 3.0 : -1.0);
	output.position = float4(position, 0, 1);
	output.uv = float2((position.x + 1.0) * 0.5, 1.0 - (position.y + 1.0) * 0.5);

	return output;
}

#endif