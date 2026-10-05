#pragma once
#include <UTILS/Singleton.hpp>

#include "Resources/RenderView.hpp"
#include "Resources/Texture2D.hpp"

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "D3D11Device.hpp"

#include <Windows.h>

namespace rend {
	class Renderer : public UTILS::Singleton<rend::Renderer> {
		rend::Texture2DHandle backBufferTexture_ = nullptr;
		rend::RenderViewHandle backBufferRenderView_ = nullptr;

		bool createRTV();
		void releaseRTV();

		void render();
		bool resizeBefore(UINT width, UINT height);
		bool resizeAfter(UINT width, UINT height);
		bool initRenderObjects();
		void shutdownRenderObjects();

		rend::D3D11Device* d3d11Device_ = nullptr;
		rend::ResourceManager* resourceManager_ = nullptr;
		rend::ShaderManager* shaderManager_ = nullptr;
	public:
		~Renderer() override;

		bool init(HWND window);

		bool resize(UINT width, UINT height);
		bool isOccluded() const;

		rend::D3D11Device* getD3D11Device() const { return this->d3d11Device_; }
		rend::ResourceManager* getResourceManager() const { return this->resourceManager_; }
		rend::ShaderManager* getShaderManager() const { return this->shaderManager_; }
		rend::RenderViewHandle getBackBufferRenderView() const { return this->backBufferRenderView_; }
	};
}