#include "D3D11Device.hpp"

#include <rend/Common/RefCounter.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcommon.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

bool rend::D3D11Device::createDevice() {
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
		result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels + skip, ARRAYSIZE(featureLevels) - skip, D3D11_SDK_VERSION, device.getAddressOf(), nullptr, deviceContext.getAddressOf());
		if (result != E_INVALIDARG)
			break;
	}

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create D3D11 device! HRESULT: 0x%08X", result);
		return false;
	}

	result = device->QueryInterface(IID_PPV_ARGS(&this->device1_));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to query ID3D11Device1 interface! HRESULT: 0x%08X", result);
		return false;
	}

	result = deviceContext->QueryInterface(IID_PPV_ARGS(&this->deviceContext1_));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to query ID3D11DeviceContext1 interface! HRESULT: 0x%08X", result);
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

bool rend::D3D11Device::init() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing renderer...");

	if (!this->createDevice())
		return false;

	this->isInitialized_ = true;

	if (this->initCallback_ && !this->initCallback_()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Renderer initialization callback failed!");
		return false;
	}

	return true;
}