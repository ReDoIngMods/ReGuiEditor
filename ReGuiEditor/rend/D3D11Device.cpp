#include "D3D11Device.hpp"

#include <rend/Common/RefCounter.hpp>

#include <DirectXColors.h>
#include <Windows.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcommon.h>
#include <dxgi.h>
#include <dxgiformat.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

bool rend::D3D11Device::createDeviceAndSwapChain(HWND window) {
	RECT clientRect;
	if (!GetClientRect(window, &clientRect)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get client rect! Error: %lu", GetLastError());
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.BufferCount = 2;
	desc.BufferDesc.Width = static_cast<UINT>(clientRect.right - clientRect.left);
	desc.BufferDesc.Height = static_cast<UINT>(clientRect.bottom - clientRect.top);
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = window;
	desc.SampleDesc.Count = 1;
	desc.Windowed = TRUE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	constexpr D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG) || defined(DEBUG)
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	rend::RefCountPtr<ID3D11Device> device = nullptr;
	rend::RefCountPtr<ID3D11DeviceContext> deviceContext = nullptr;

	// Pre-11.1 runtimes reject a feature level array containing 11_1 with E_INVALIDARG.
	HRESULT result = E_FAIL;
	for (UINT skip = 0; skip <= 1; ++skip) {
		result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels + skip, ARRAYSIZE(featureLevels) - skip, D3D11_SDK_VERSION, &desc, &this->swapChain_, device.getAddressOf(), nullptr, deviceContext.getAddressOf());
		if (result != E_INVALIDARG)
			break;
	}

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create D3D11 device and swap chain! HRESULT: 0x%X", result);
		return false;
	}

	result = device->QueryInterface(IID_PPV_ARGS(&this->device1_));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to query ID3D11Device1 interface! HRESULT: 0x%X", result);
		return false;
	}

	result = deviceContext->QueryInterface(IID_PPV_ARGS(&this->deviceContext1_));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to query ID3D11DeviceContext1 interface! HRESULT: 0x%X", result);
		return false;
	}

	rend::RefCountPtr<IDXGIFactory> factory = nullptr;
	if (FAILED(this->swapChain_->GetParent(IID_PPV_ARGS(&factory)))) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get IDXGIFactory from swap chain! HRESULT: 0x%X", result);
		return false;
	}

	factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

	return true;
}

bool rend::D3D11Device::createRenderTarget() {
	rend::RefCountPtr<ID3D11Texture2D> backBuffer = nullptr;
	HRESULT result = this->swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get back buffer! HRESULT: 0x%X", result);
		return false;
	}

	result = this->device1_->CreateRenderTargetView(backBuffer, nullptr, &this->renderTargetView_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create render target view! HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

rend::D3D11Device::~D3D11Device() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Cleaning up renderer...");

	if (this->isInitialized_ && this->shutdownCallback_)
		this->shutdownCallback_();

	this->isInitialized_ = false;

	if (this->deviceContext1_) {
		this->deviceContext1_->ClearState();
		this->deviceContext1_->Flush();
	}

#if defined(_DEBUG) || defined(DEBUG)
	if (this->device1_) {
		rend::RefCountPtr<ID3D11Debug> debug = nullptr;
		if (SUCCEEDED(this->device1_->QueryInterface(IID_PPV_ARGS(&debug))))
			debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
	}
#endif
}

bool rend::D3D11Device::init(HWND window) {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing renderer...");

	this->windowHandle_ = window;

	if (!this->createDeviceAndSwapChain(window) || !this->createRenderTarget())
		return false;

	this->isInitialized_ = true;

	if (this->initCallback_ && !this->initCallback_()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Renderer initialization callback failed!");
		return false;
	}

	return true;
}

bool rend::D3D11Device::resize(UINT width, UINT height) {
	if (!this->isInitialized_ || width == 0 || height == 0)
		return false;

	if (this->resizeBeforeCallback_)
		this->resizeBeforeCallback_(width, height);

	this->deviceContext1_->OMSetRenderTargets(0, nullptr, nullptr);
	this->renderTargetView_.reset();

	HRESULT result = this->swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resize swap chain buffers! HRESULT: 0x%X", result);
		return false;
	}

	if (!this->createRenderTarget())
		return false;

	if (this->resizeAfterCallback_)
		this->resizeAfterCallback_(width, height);

	return true;
}

bool rend::D3D11Device::beginFrame() {
	if (!this->isInitialized_ || !this->renderTargetView_)
		return false;

	this->deviceContext1_->OMSetRenderTargets(1, this->renderTargetView_.getAddressOf(), nullptr);
	this->deviceContext1_->ClearRenderTargetView(this->renderTargetView_, DirectX::Colors::Black);
	return true;
}

bool rend::D3D11Device::endFrame() {
	if (!this->isInitialized_)
		return false;

	HRESULT result = this->swapChain_->Present(1, 0);
	if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Device lost or reset! HRESULT: 0x%X", result);
		return false;
	}

	return SUCCEEDED(result);
}

bool rend::D3D11Device::isOccluded() const {
	return this->swapChain_ && this->swapChain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
}