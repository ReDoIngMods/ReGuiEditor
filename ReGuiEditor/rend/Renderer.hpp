#pragma once
#include <UTILS/Singleton.hpp>

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "D3D11Device.hpp"

#include <memory>

namespace rend {
	class Renderer : public UTILS::Singleton<rend::Renderer> {
		void render();
		bool initRenderObjects();

		std::shared_ptr<rend::D3D11Device> d3d11Device_ = nullptr;
		std::shared_ptr<rend::ResourceManager> resourceManager_ = nullptr;
		std::shared_ptr<rend::ShaderManager> shaderManager_ = nullptr;
	public:
		bool init();
	};
}