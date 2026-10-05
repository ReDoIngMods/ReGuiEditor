#pragma once
#include <GUI/MyGuiDX11/MGTexture.hpp>

#include <rend/Common/RefCounter.hpp>

#include <rend/FullscreenPass.hpp>

#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/SamplerState.hpp>

#include <string>

class BackgroundImageHelper {
	rend::FullscreenPass fullscreenPass_;
	rend::SamplerStateHandle samplerState_;

	MyGUICustom::DirectX11Texture* backgroundTexture_ = nullptr;
public:
	bool initialize();
	void cleanup();

	void setBackgroundTexture(const std::string& path);

	void render(rend::RenderView* renderView, float width, float height);
};