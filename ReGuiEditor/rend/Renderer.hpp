#pragma once
#include <Modules/Misc/Singleton.hpp>

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "D3D11Device.hpp"

#include <memory>

namespace rend {
	class Renderer : public ReGuiEditor::Singleton<rend::Renderer> {
		void render();
		bool initRenderObjects();

		std::shared_ptr<rend::D3D11Device> d3d11Device_ = nullptr;
		std::shared_ptr<rend::ResourceManager> resourceManager_ = nullptr;
		std::shared_ptr<rend::ShaderManager> shaderManager_ = nullptr;
	public:
		bool init();
	};
}