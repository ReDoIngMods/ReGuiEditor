#include "SwapChain.hpp"

#include <rend/D3D11Device.hpp>
#include <rend/ResourceManager.hpp>

#include <rend/Common/RefCounter.hpp>

#include <rend/Common/Color.hpp>

#include <UTILS/Strings.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <dxgi1_5.h>
#include <dxgiformat.h>

#include <dxgi1_2.h>
#include <string>

bool rend::SwapChain::createRenderTarget() {
	if (!this->swapChain_) {
		CONSOLE_ERROR("SwapChain is null when creating render target view!");
		return false;
	}

	rend::RefCountPtr<ID3D11Texture2D> backBuffer = nullptr;
	HRESULT result = this->swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.getAddressOf()));
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to get back buffer from swap chain! HRESULT: 0x%08X", result);
		return false;
	}

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
	this->backBuffer_ = resourceManager.createTexture2DNative(backBuffer);
	if (!this->backBuffer_) {
		CONSOLE_ERROR("Failed to create Texture2D from back buffer!");
		return false;
	}

	const HWND window = this->getWindowHandle();
	const std::string backbufferDebugName = UTILS::formatString("SwapChain BackBuffer (%p, %dx%d)", window, this->backBuffer_->getWidth(), this->backBuffer_->getHeight());
	this->backBuffer_->setDebugName(backbufferDebugName);

	this->renderTargetView_ = resourceManager.createRenderView(this->backBuffer_);
	if (!this->renderTargetView_) {
		CONSOLE_ERROR("Failed to create RenderView from back buffer!");
		return false;
	}

	const std::string renderViewDebugName = UTILS::formatString("SwapChain RenderView (%p, %dx%d)", window, this->backBuffer_->getWidth(), this->backBuffer_->getHeight());
	this->renderTargetView_->setDebugName(renderViewDebugName);

	return true;
}

void rend::SwapChain::cleanupRenderTarget() {
	this->renderTargetView_.reset();
	this->backBuffer_.reset();
}

bool rend::SwapChain::init(void* window) {
	HWND hwnd = reinterpret_cast<HWND>(window);
	if (!hwnd || !IsWindow(hwnd)) {
		CONSOLE_ERROR("Invalid window handle provided to SwapChain::init!");
		return false;
	}

	this->windowHandle_ = hwnd;

	RECT clientRect;
	if (!GetClientRect(hwnd, &clientRect)) {
		CONSOLE_ERROR("Failed to get client rect! Error: %lu", GetLastError());
		return false;
	}

	UINT width = static_cast<UINT>(clientRect.right - clientRect.left);
	UINT height = static_cast<UINT>(clientRect.bottom - clientRect.top);

	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	
	rend::RefCountPtr<IDXGIDevice> dxgiDevice = nullptr;
	HRESULT result = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to query IDXGIDevice interface! HRESULT: 0x%08X", result);
		return false;
	}

	rend::RefCountPtr<IDXGIAdapter> dxgiAdapter = nullptr;
	result = dxgiDevice->GetAdapter(&dxgiAdapter);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to get IDXGIAdapter! HRESULT: 0x%08X", result);
		return false;
	}

	rend::RefCountPtr<IDXGIFactory2> dxgiFactory2 = nullptr;
	result = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory2));
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to get IDXGIFactory! HRESULT: 0x%08X", result);
		return false;
	}

	{
		rend::RefCountPtr<IDXGIFactory5> dxgiFactory5 = nullptr;
		if (SUCCEEDED(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&dxgiFactory5)))) {
			BOOL allowTearing = FALSE;
			HRESULT result = dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
			if (FAILED(result))
				CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to check for tearing support! HRESULT: 0x%08X", result);
			else
				this->tearingEnabled_ = allowTearing == TRUE;
		} else {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to query IDXGIFactory5 interface! Tearing support check will be skipped.");
		}
	}

	DXGI_SWAP_CHAIN_DESC1 desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 2;
	desc.SampleDesc.Count = 1;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	desc.Flags = this->tearingEnabled_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	desc.Stereo = FALSE;

	result = dxgiFactory2->CreateSwapChainForHwnd(device, hwnd, &desc, nullptr, nullptr, &this->swapChain_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create swap chain! HRESULT: 0x%08X", result);
		return false;
	}

	dxgiFactory2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	
	return this->createRenderTarget();
}

bool rend::SwapChain::resize(UINT width, UINT height) {
	if (!this->swapChain_ || width == 0 || height == 0)
		return false;

	this->cleanupRenderTarget();

	DXGI_SWAP_CHAIN_DESC1 desc;
	HRESULT result = this->swapChain_->GetDesc1(&desc);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to get swap chain description! HRESULT: 0x%08X", result);
		return false;
	}

	result = this->swapChain_->ResizeBuffers(0, width, height, desc.Format, desc.Flags);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to resize swap chain buffers! HRESULT: 0x%08X", result);
		return false;
	}

	return this->createRenderTarget();
}

bool rend::SwapChain::present(bool vsyncEnabled) {
	if (!this->swapChain_)
		return false;

	UINT syncInterval = vsyncEnabled ? 1 : 0;
	UINT presentFlags = vsyncEnabled ? 0 : (this->tearingEnabled_ ? DXGI_PRESENT_ALLOW_TEARING : 0);

	HRESULT result = this->swapChain_->Present(syncInterval, presentFlags);
	if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
		CONSOLE_ERROR("Device lost or reset! HRESULT: 0x%08X", result);
		return false;
	}

	return SUCCEEDED(result);
}

bool rend::SwapChain::isOccluded() const {
	return this->swapChain_ && this->swapChain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
}

bool rend::SwapChain::clearRenderTarget(rend::Color color) const {
	if (!this->renderTargetView_) {
		CONSOLE_ERROR("Render target view is null when clearing render target!");
		return false;
	}

	rend::D3D11Device::getInstance().getDeviceContext1()->ClearRenderTargetView(this->renderTargetView_->getRenderTargetView(), color.components);
	return true;
}