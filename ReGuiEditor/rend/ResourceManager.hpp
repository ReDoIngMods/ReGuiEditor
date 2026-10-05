#pragma once
#include <UTILS/Singleton.hpp>

#include "Common/Format.hpp"
#include "Common/RefCounter.hpp"
#include "Common/SamplerAddressMode.hpp"

#include "Resources/Buffers/ConstantBuffer.hpp"
#include "Resources/Buffers/IndexBuffer.hpp"
#include "Resources/Buffers/VertexBuffer.hpp"

#include "Resources/BlendState.hpp"
#include "Resources/DepthStencilState.hpp"
#include "Resources/PipelineContext.hpp"
#include "Resources/RasterizerState.hpp"
#include "Resources/SamplerState.hpp"

#include "Resources/RenderView.hpp"
#include "Resources/Texture2D.hpp"

#include "Resources/Shaders/InputLayout.hpp"
#include "Resources/Shaders/PixelShader.hpp"
#include "Resources/Shaders/ShaderResource.hpp"
#include "Resources/Shaders/VertexShader.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace rend {
	class Renderer;

	class ResourceManager : public UTILS::Singleton<ResourceManager> {
		friend class rend::Renderer;
		friend class rend::Resource;

		std::atomic<uint32_t> resourceCount_ = 0; // Seperate to prevent a deadlock.
		std::vector<rend::Resource*> resources_;
		std::mutex resourcesMutex_;

		bool registerResource(rend::Resource* resource);
		bool unregisterResource(rend::Resource* resource);
	public:
		ResourceManager() = default;
		~ResourceManager() override { this->cleanup(); }

		void cleanup();

		rend::Texture2DHandle createTexture2D(uint32_t width, uint32_t height, rend::Format format, rend::Texture2DFlags flags);
		rend::Texture2DHandle createTexture2DFromBackBuffer();

		rend::RenderViewHandle createRenderView(rend::Texture2D* texture);

		rend::VertexShaderHandle createVertexShader(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros = {});
		rend::PixelShaderHandle createPixelShader(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros = {});

		template <typename T>
		rend::RefCountPtr<T> createShader(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros = {}) {
			static_assert(std::is_base_of<rend::ShaderResource, T>::value, "T must be a subclass of rend::ShaderResource");

			if constexpr (std::is_same<T, rend::VertexShader>::value) {
				return this->createVertexShader(shaderName, entrypoint, macros);
			} else if constexpr (std::is_same<T, rend::PixelShader>::value) {
				return this->createPixelShader(shaderName, entrypoint, macros);
			} else {
				static_assert(false, "Unsupported shader type");
			}
		}

		rend::InputLayoutHandle createInputLayout(const rend::InputLayoutDesc& inputElementDescs, rend::VertexShader* vertexShader);

		rend::IndexBufferHandle createIndexBuffer(rend::IndexBufferFormat format, uint32_t indexCount = 0, const void* initialData = nullptr);
		rend::VertexBufferHandle createVertexBuffer(uint32_t vertexSize, uint32_t vertexCount = 0, const void* initialData = nullptr);
		rend::ConstantBufferHandle createConstantBuffer(uint32_t bufferSize, const void* initialData = nullptr);

		rend::RasterizerStateHandle createRasterizerState(const rend::RasterizerDesc& desc);
		rend::RasterizerStateHandle createDefaultRasterizerState();

		rend::BlendStateHandle createBlendState(const rend::BlendDesc& desc);
		rend::BlendStateHandle createDefaultBlendState();

		rend::SamplerStateHandle createSamplerState(const rend::SamplerDesc& desc);
		rend::SamplerStateHandle createLinearSamplerState(rend::SamplerAddressMode addressMode = rend::SamplerAddressMode::Wrap);
		rend::SamplerStateHandle createPointSamplerState(rend::SamplerAddressMode addressMode = rend::SamplerAddressMode::Wrap);
		rend::SamplerStateHandle createAnisotropicSamplerState(rend::SamplerAddressMode addressMode = rend::SamplerAddressMode::Wrap, uint32_t maxAnisotropy = 16);

		rend::DepthStencilStateHandle createDepthStencilState(const rend::DepthStencilDesc& desc);
		rend::DepthStencilStateHandle createDefaultDepthStencilState();

		rend::PipelineContextHandle createPipelineContext(const std::string& name = "");

		size_t getResourceCount() {
			return this->resourceCount_.load(std::memory_order_relaxed);
		}

		std::vector<rend::Resource*> getResources() {
			std::lock_guard<std::mutex> lock(this->resourcesMutex_);
			std::vector<rend::Resource*> resourcesCopy;
			resourcesCopy.reserve(this->resources_.size());

			for (rend::Resource* resource : this->resources_) {
				resource->AddRef();
				resourcesCopy.push_back(resource);
			}

			return resourcesCopy;
		}
	};
}