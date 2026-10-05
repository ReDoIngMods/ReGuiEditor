#pragma once
#include <UTILS/Singleton.hpp>

#include <rend/Common/RefCounter.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>

#include <functional>
#include <utility>

namespace rend {
	class D3D11Device : public UTILS::Singleton<D3D11Device> {
		rend::RefCountPtr<IDXGISwapChain> swapChain_;
		rend::RefCountPtr<ID3D11Device1> device1_;
		rend::RefCountPtr<ID3D11DeviceContext1> deviceContext1_;
		rend::RefCountPtr<ID3D11RenderTargetView> renderTargetView_;

		HWND windowHandle_ = nullptr;
		bool isInitialized_ = false;

		std::function<void()> renderCallback_;
		std::function<bool(UINT, UINT)> resizeBeforeCallback_;
		std::function<bool(UINT, UINT)> resizeAfterCallback_;
		std::function<bool()> initCallback_;
		std::function<void()> shutdownCallback_;

		bool createDeviceAndSwapChain(HWND window);
		bool createRenderTarget();
	public:
		D3D11Device() = default;
		~D3D11Device() override;

		bool init(HWND window);

		bool resize(UINT width, UINT height);
		
		bool beginFrame();
		bool endFrame();

		bool isOccluded() const;

		void setRenderCallback(std::function<void()> callback) { this->renderCallback_ = std::move(callback); }
		void setResizeBeforeCallback(std::function<bool(UINT, UINT)> callback) { this->resizeBeforeCallback_ = std::move(callback); }
		void setResizeAfterCallback(std::function<bool(UINT, UINT)> callback) { this->resizeAfterCallback_ = std::move(callback); }
		void setInitCallback(std::function<bool()> callback) { this->initCallback_ = std::move(callback); }
		void setShutdownCallback(std::function<void()> callback) { this->shutdownCallback_ = std::move(callback); }

		rend::RefCountPtr<IDXGISwapChain> getSwapChain() const { return this->swapChain_; }
		rend::RefCountPtr<ID3D11Device1> getDevice1() const { return this->device1_; }
		rend::RefCountPtr<ID3D11DeviceContext1> getDeviceContext1() const { return this->deviceContext1_; }
		rend::RefCountPtr<ID3D11RenderTargetView> getRenderTargetView() const { return this->renderTargetView_; }

		HWND getWindowHandle() const { return this->windowHandle_; }
	};
}