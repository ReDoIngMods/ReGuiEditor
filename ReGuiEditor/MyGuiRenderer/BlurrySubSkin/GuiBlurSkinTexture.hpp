#pragma once
#include "../RenderPlatform/RPTexture.hpp"

#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/RenderView.hpp>

#include <string>

class GuiBlurSkinTexture {
	RPTexture* texture_ = nullptr;
	rend::RenderViewHandle renderView_;
	std::string name_;
public:
	GuiBlurSkinTexture() = default;
	~GuiBlurSkinTexture() { this->cleanup(); }

	GuiBlurSkinTexture(const GuiBlurSkinTexture&) = delete;
	GuiBlurSkinTexture& operator=(const GuiBlurSkinTexture&) = delete;

	bool initialize(const std::string& name, int width, int height);
	void cleanup();

	bool resize(int width, int height);

	const std::string& getName() const { return this->name_; }
	rend::RenderView* getRenderView() const { return this->renderView_.get(); }
};
