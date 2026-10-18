#pragma once
#include <UTILS/Singleton.hpp>

#include <rend/Common/RefCounter.hpp>

#include <rend/FullscreenPass.hpp>

#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/SamplerState.hpp>
#include <rend/Resources/ShaderResourceView.hpp>

#include <cstdint>
#include <string>

class GuiBlurManager : public UTILS::Singleton<GuiBlurManager> {
	uint32_t fullWidth_ = 0;
	uint32_t fullHeight_ = 0;

	uint32_t downsample_ = 2;

	uint32_t lowWidth_ = 0;
	uint32_t lowHeight_ = 0;

	rend::RenderViewHandle captureView_;
	rend::RenderViewHandle pingView_;
	rend::RenderViewHandle pongView_;

	// One pass per pixel shader, each with its own BlurParameters constant buffer.
	rend::FullscreenPass copyPass_;
	rend::FullscreenPass blurHorizontalPass_;
	rend::FullscreenPass blurVerticalPass_;

	rend::SamplerStateHandle samplerState_;

	struct alignas(16) BlurParameters {
		float texelSizeX;
		float texelSizeY;
	};

	bool createPass(rend::FullscreenPass& pass, const char* entryPoint, const char* debugName);
	bool createRenderObjects();

	rend::RenderViewHandle createRenderTarget(const std::string& name) const;
	bool createRenderTargets();
	void cleanupRenderTargets();

	void blurPass(const rend::ShaderResourceView* sourceView, rend::RenderView* destinationView, rend::FullscreenPass& pass, float texelSizeX, float texelSizeY);
public:
	bool initialize(uint32_t fullWidth, uint32_t fullHeight, uint32_t downsample = 2);
	void cleanup();

	bool resize(uint32_t fullWidth, uint32_t fullHeight);

	void captureAndBlur(const rend::ShaderResourceView* sourceView);

	const rend::ShaderResourceView* getResultShaderResourceView() const { return this->pongView_.get(); }
};
