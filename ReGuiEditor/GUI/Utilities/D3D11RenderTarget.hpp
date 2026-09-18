#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgiformat.h>

namespace RenderTargetUtils {
	bool createColorfulRTV(UINT width, UINT height, DXGI_FORMAT format, ID3D11Texture2D*& outputTexture, ID3D11RenderTargetView*& outputRTV, ID3D11ShaderResourceView*& outputSRV);
}