#pragma once
#include <UTILS/Singleton.hpp>

#include <d3d11.h>

#include <cstdint>

class GuiBlurManager : public UTILS::Singleton<GuiBlurManager> {
	uint32_t fullWidth_ = 0;
	uint32_t fullHeight_ = 0;

	uint32_t downsample_ = 2;

	uint32_t lowWidth_ = 0;
	uint32_t lowHeight_ = 0;

	ID3D11Texture2D* captureTexture = nullptr;
	ID3D11RenderTargetView* captureTargetView = nullptr;
	ID3D11ShaderResourceView* captureShaderResourceView = nullptr;

	ID3D11Texture2D* pingTexture = nullptr;
	ID3D11RenderTargetView* pingTargetView = nullptr;
	ID3D11ShaderResourceView* pingShaderResourceView = nullptr;

	ID3D11Texture2D* pongTexture = nullptr;
	ID3D11RenderTargetView* pongTargetView = nullptr;
	ID3D11ShaderResourceView* pongShaderResourceView = nullptr;

	ID3D11VertexShader* shaderVertex_ = nullptr;
	ID3D11PixelShader* shaderPixelBlurHorizontal_ = nullptr;
	ID3D11PixelShader* shaderPixelBlurVertical_ = nullptr;
	ID3D11PixelShader* shaderPixelCopy_ = nullptr;

	ID3D11SamplerState* samplerState_ = nullptr;
	ID3D11Buffer* constantBuffer_ = nullptr;

	struct alignas(16) ConstantBuffer {
		float texelSizeX;
		float texelSizeY;
	};

	void cleanupRenderTargets(ID3D11Texture2D*& texture, ID3D11RenderTargetView*& targetView, ID3D11ShaderResourceView*& shaderResourceView);

	bool createRenderObjects();
	void cleanupRenderObjects();

	void blurPass(ID3D11ShaderResourceView* sourceView, ID3D11RenderTargetView* destinationView, ID3D11PixelShader* pixelShader, float texelSizeX, float texelSizeY);
public:
	GuiBlurManager() = default;
	~GuiBlurManager() { this->cleanup(); }

	GuiBlurManager(const GuiBlurManager&) = delete;
	GuiBlurManager& operator=(const GuiBlurManager&) = delete;
	GuiBlurManager(GuiBlurManager&&) = delete;
	GuiBlurManager& operator=(GuiBlurManager&&) = delete;

	bool initialize(uint32_t fullWidth, uint32_t fullHeight, uint32_t downsample = 2);
	void cleanup();

	bool resize(uint32_t fullWidth, uint32_t fullHeight);

	void captureAndBlur(ID3D11ShaderResourceView* sourceView);

	ID3D11ShaderResourceView* getResultShaderResourceView() const { return this->pongShaderResourceView; }
};
