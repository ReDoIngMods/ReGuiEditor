#pragma once
#include <rend/Common/RefCounter.hpp>

#include <rend/Common/Color.hpp>

#include "RenderView.hpp"
#include "Resource.hpp"
#include "Texture2D.hpp"

#include <Windows.h>
#include <dxgi1_2.h>

namespace rend {
	class SwapChain : public rend::RefCounter<rend::Resource> {
		rend::RefCountPtr<IDXGISwapChain1> swapChain_ = nullptr;
		rend::RefCountPtr<rend::Texture2D> backBuffer_ = nullptr;
		rend::RefCountPtr<rend::RenderView> renderTargetView_ = nullptr;
		HWND windowHandle_ = nullptr;

		bool tearingEnabled_ = false;

		bool createRenderTarget();
		void cleanupRenderTarget();
	public:
		[[nodiscard]] bool init(void* window);
		[[nodiscard]] bool resize(UINT width, UINT height);
		[[nodiscard]] bool present(bool vsyncEnabled = false);
		[[nodiscard]] bool isOccluded() const;

		[[nodiscard]] bool clearRenderTarget(rend::Color color) const;

		[[nodiscard]] rend::RefCountPtr<IDXGISwapChain1> getSwapChain() const { return this->swapChain_; }
		[[nodiscard]] rend::RefCountPtr<rend::RenderView> getRenderTargetView() const { return this->renderTargetView_; }
		[[nodiscard]] HWND getWindowHandle() const { return this->windowHandle_; }
	};

	typedef rend::RefCountPtr<rend::SwapChain> SwapChainHandle;
}