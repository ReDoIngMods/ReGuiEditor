#pragma once
#include <Windows.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>

class D3D11Device {
	HWND hwnd_ = nullptr;
	IDXGISwapChain1* swapChain_ = nullptr;
	ID3D11Device1* device_ = nullptr;
	ID3D11DeviceContext1* context_ = nullptr;
	ID3D11RenderTargetView* renderTargetView_ = nullptr;
	IDXGIFactory5* dxgiFactory_ = nullptr;

	int lastWidth_ = 0;
	int lastHeight_ = 0;
	bool tearingSupport_ = false;

	bool isInitialized_ = false;

	bool createRenderTargetView();
	void cleanupRenderTargetView();
public:
	bool initialize(HWND hwnd);
	void cleanup();
	
	bool isInitialized() const { return this->isInitialized_; }
	
	bool resize(int width, int height);

	void updateRenderTarget();
	void present(bool vsyncEnabled = true);

	bool isOccluded() const;

	HWND getHwnd() const { return this->hwnd_; }
	IDXGISwapChain* getSwapChain() const { return this->swapChain_; }
	ID3D11Device* getDevice() const { return this->device_; }
	ID3D11DeviceContext* getContext() const { return this->context_; }
	ID3D11RenderTargetView* getRenderTargetView() const { return this->renderTargetView_; }
};