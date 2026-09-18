#pragma once
#include <GUI/MyGuiDX11/Texture.hpp>

#include <d3d11.h>

#include <string>

class GuiBlurSkinTexture {
	MyGUICustom::DirectX11Texture* texture_ = nullptr;
	std::string name_;
public:
	GuiBlurSkinTexture() = default;
	~GuiBlurSkinTexture() { this->cleanup(); }

	GuiBlurSkinTexture(const GuiBlurSkinTexture&) = delete;
	GuiBlurSkinTexture& operator=(const GuiBlurSkinTexture&) = delete;

	bool initialize(const std::string& name, int width, int height);
	void cleanup();

	void update(ID3D11Texture2D* texture, ID3D11ShaderResourceView* shaderResourceView);

	const std::string& getName() const { return this->name_; }
};
