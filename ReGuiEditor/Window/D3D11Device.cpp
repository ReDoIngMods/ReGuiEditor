#include "D3D11Device.hpp"
#pragma comment(lib, "d3d11.lib")

#include <Windows.h>

#include <DirectXColors.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcommon.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <dxgiformat.h>

bool D3D11Device::createRenderTargetView() {
	ID3D11Texture2D* backBuffer = nullptr;
	HRESULT result = this->swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to get back buffer from swap chain.");
		return false;
	}

	result = this->device_->CreateRenderTargetView(backBuffer, nullptr, &this->renderTargetView_);
	backBuffer->Release();
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create render target view.");
		return false;
	}

	this->context_->OMSetRenderTargets(1, &this->renderTargetView_, nullptr);
	return true;
}

void D3D11Device::cleanupRenderTargetView() {
	if (this->renderTargetView_) {
		this->renderTargetView_->Release();
		this->renderTargetView_ = nullptr;
	}
}

bool D3D11Device::initialize(HWND hwnd) {
	this->hwnd_ = hwnd;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0,
	};

	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &device, nullptr, &context);
	if (FAILED(result))
		result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, creationFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &device, nullptr, &context);

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create ID3D11Device");
		return false;
	}

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, L"Created ID3D11Device and ID3D11DeviceContext.");

	result = device->QueryInterface(IID_PPV_ARGS(&this->device_));
	device->Release();
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to query ID3D11Device1");
		context->Release();
		return false;
	}

	result = context->QueryInterface(IID_PPV_ARGS(&this->context_));
	context->Release();
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to query ID3D11DeviceContext1");
		this->device_->Release();
		this->device_ = nullptr;
		return false;
	}

	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIAdapter* dxgiAdapter = nullptr;

	result = this->device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
	if (SUCCEEDED(result)) result = dxgiDevice->GetAdapter(&dxgiAdapter);
	if (SUCCEEDED(result)) result = dxgiAdapter->GetParent(IID_PPV_ARGS(&this->dxgiFactory_));

	if (dxgiDevice) dxgiDevice->Release();
	if (dxgiAdapter) dxgiAdapter->Release();

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to obtain DXGI factory.");
		return false;
	}

	BOOL allowTearing = FALSE;
	this->dxgiFactory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	this->tearingSupport_ = allowTearing != FALSE;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	if (this->tearingSupport_)
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	result = this->dxgiFactory_->CreateSwapChainForHwnd(this->device_, hwnd, &swapChainDesc, nullptr, nullptr, &this->swapChain_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create swap chain.");
		return false;
	}

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, L"Created Swap Chain.");

	RECT rect;
	if (!GetClientRect(hwnd, &rect)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to get client rect of window.");
		return false;
	}

	this->lastWidth_ = rect.right - rect.left;
	this->lastHeight_ = rect.bottom - rect.top;

	result = this->dxgiFactory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to make window association.");
		return false;
	}

	if (!this->createRenderTargetView()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create render target view.");
		return false;
	}

	this->isInitialized_ = true;
	return true;
}

void D3D11Device::cleanup() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, L"Cleaning up D3D11 device...");

	this->isInitialized_ = false;
	this->cleanupRenderTargetView();

	if (this->swapChain_) {
		CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, L"Releasing ID3D11SwapChain1...");

		this->swapChain_->Release();
		this->swapChain_ = nullptr;
	}

	if (this->context_) {
		CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, L"Releasing ID3D11DeviceContext1...");

		this->context_->ClearState();
		this->context_->Flush();
		this->context_->Release();
		this->context_ = nullptr;
	}

	if (this->device_) {
		CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, L"Releasing ID3D11Device...");
#if defined(_DEBUG) || defined(DEBUG)
		ID3D11Debug* debug = nullptr;
		if (SUCCEEDED(this->device_->QueryInterface(IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
			debug->Release();
		}
#endif
		this->device_->Release();
		this->device_ = nullptr;
	}

	if (this->dxgiFactory_) {
		CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, L"Releasing IDXGIFactory5...");

		this->dxgiFactory_->Release();
		this->dxgiFactory_ = nullptr;
	}
}

bool D3D11Device::resize(int width, int height) {
	if (width == this->lastWidth_ && height == this->lastHeight_)
		return true;

	this->cleanupRenderTargetView();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	this->swapChain_->GetDesc1(&swapChainDesc);

	HRESULT result = this->swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swapChainDesc.Flags);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to resize swap chain buffers.");
		return false;
	}

	this->lastWidth_ = width;
	this->lastHeight_ = height;

	return this->createRenderTargetView();
}

void D3D11Device::updateRenderTarget() {
	this->context_->ClearRenderTargetView(this->renderTargetView_, DirectX::Colors::Black);
	this->context_->OMSetRenderTargets(1, &this->renderTargetView_, nullptr);
}

void D3D11Device::present(bool vsyncEnabled) {
	HRESULT result = S_OK;
	if (vsyncEnabled)
		result = this->swapChain_->Present(1, 0);
	else
		result = this->swapChain_->Present(0, this->tearingSupport_ ? DXGI_PRESENT_ALLOW_TEARING : 0);

	if (FAILED(result))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to present swap chain.");
}

bool D3D11Device::isOccluded() const {
	if (!this->swapChain_)
		return false;

	return this->swapChain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
}