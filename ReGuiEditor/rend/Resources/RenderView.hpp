#pragma once
#include "ShaderResourceView.hpp"
#include "Texture2D.hpp"

#include <rend/Common/RefCounter.hpp>

#include <d3d11.h>

namespace rend {
	class RenderView : public rend::RefCounter<rend::ShaderResourceView> {
		rend::Texture2DHandle texture_;
		rend::RefCountPtr<ID3D11RenderTargetView> renderTargetView_ = nullptr;
	public:
		[[nodiscard]] bool init(rend::Texture2D* texture);

		[[nodiscard]] rend::Texture2DHandle getTexture() const { return this->texture_; }
		[[nodiscard]] rend::RefCountPtr<ID3D11RenderTargetView> getRenderTargetView() const { return this->renderTargetView_; }
	};

	typedef rend::RefCountPtr<rend::RenderView> RenderViewHandle;
}