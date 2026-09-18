#pragma once
#include <GUI/MyGuiDX11/Texture.hpp>

#include <d3d11.h>

#include <string>

class BackgroundImageHelper {
	ID3D11VertexShader* vertexShader_ = nullptr;
	ID3D11PixelShader* pixelShader_ = nullptr;
	ID3D11SamplerState* samplerState_ = nullptr;
	ID3D11Buffer* constantBuffer_ = nullptr;

	MyGUICustom::DirectX11Texture* backgroundTexture_ = nullptr;
public:
	bool initialize();
	void cleanup();

	void setBackgroundTexture(const std::string& path);

	void render();
};