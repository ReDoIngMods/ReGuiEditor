#pragma once
#include "RenderPlatform/RPTexture.hpp"

#include <rend/FullscreenPass.hpp>

#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/SamplerState.hpp>

#include <string>

class BackgroundImageHelper {
	rend::FullscreenPass fullscreenPass_;
	rend::SamplerStateHandle samplerState_;

	RPTexture* backgroundTexture_ = nullptr;
public:
	bool initialize();
	void cleanup();

	void setBackgroundTexture(const std::string& path);

	void render(rend::RenderView* renderView, float width, float height);
};