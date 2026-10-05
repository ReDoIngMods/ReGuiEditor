#pragma once
#include <UTILS/Singleton.hpp>

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "D3D11Device.hpp"

#include <Windows.h>

namespace rend {
	class Renderer : public UTILS::Singleton<rend::Renderer> {
		void render();
		bool initRenderObjects();
		void shutdownRenderObjects();

		rend::D3D11Device* d3d11Device_ = nullptr;
		rend::ResourceManager* resourceManager_ = nullptr;
		rend::ShaderManager* shaderManager_ = nullptr;
	public:
		~Renderer() override;

		bool init(HWND window);

		rend::D3D11Device* getD3D11Device() const { return this->d3d11Device_; }
		rend::ResourceManager* getResourceManager() const { return this->resourceManager_; }
		rend::ShaderManager* getShaderManager() const { return this->shaderManager_; }
	};
}