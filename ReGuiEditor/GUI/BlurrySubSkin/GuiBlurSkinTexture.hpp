#pragma once
#include <GUI/MyGuiDX11/MGTexture.hpp>

#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/RenderView.hpp>

#include <string>

// A MyGUI texture that the blur composite is rendered into.
class GuiBlurSkinTexture {
	MyGUICustom::DirectX11Texture* texture_ = nullptr;
	rend::RenderViewHandle renderView_;
	std::string name_;
public:
	GuiBlurSkinTexture() = default;
	~GuiBlurSkinTexture() { this->cleanup(); }

	GuiBlurSkinTexture(const GuiBlurSkinTexture&) = delete;
	GuiBlurSkinTexture& operator=(const GuiBlurSkinTexture&) = delete;

	bool initialize(const std::string& name, int width, int height);
	void cleanup();

	// Recreates the backing texture (and its render view). The MyGUI texture object itself stays the same.
	bool resize(int width, int height);

	const std::string& getName() const { return this->name_; }
	rend::RenderView* getRenderView() const { return this->renderView_.get(); }
};
