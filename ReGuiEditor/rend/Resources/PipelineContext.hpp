#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>

#include <d3d11_1.h>

#include <rend/Common/RefCounter.hpp>

#include <rend/Common/PrimitiveTopology.hpp>
#include <rend/Common/Rectangle.hpp>
#include <rend/Common/Viewport.hpp>

#include "Buffers/BufferResource.hpp"
#include "Buffers/ConstantBuffer.hpp"
#include "Buffers/IndexBuffer.hpp"
#include "Buffers/VertexBuffer.hpp"

#include "BlendState.hpp"
#include "DepthStencilState.hpp"
#include "RasterizerState.hpp"
#include "RenderView.hpp"
#include "SamplerState.hpp"
#include "ShaderResourceView.hpp"

#include "Shaders/InputLayout.hpp"
#include "Shaders/PixelShader.hpp"
#include "Shaders/VertexShader.hpp"

#include "Resource.hpp"

namespace rend {
	enum class ShaderStage {
		Vertex,
		Pixel,
	};

	class PipelineContext : public rend::RefCounter<rend::Resource> {
	private:
		rend::RefCountPtr<ID3DDeviceContextState> deviceContextState_;
		rend::RefCountPtr<ID3DDeviceContextState> previousDeviceContextState_;
		
		rend::RefCountPtr<ID3D11DeviceContext1> deviceContext1_;
		rend::RefCountPtr<ID3D11Device1> device1_;

		bool isActive_ = false;
	public:
		bool init();

		bool begin();
		bool end();

		void setRasterizerState(const rend::RasterizerState* rasterizerState);
		void setBlendState(const rend::BlendState* blendState);
		void setDepthStencilState(const rend::DepthStencilState* depthStencilState);

		void setInputLayout(const rend::InputLayout* inputLayout);

		void setVertexShader(const rend::VertexShader* vertexShader);
		void setPixelShader(const rend::PixelShader* pixelShader);

		void setIndexBuffer(const rend::IndexBuffer* indexBuffer);

		void setPrimitiveTopology(rend::PrimitiveTopology topology);

		// Drawing
		void draw(uint32_t vertexCount, uint32_t startVertexLocation);
		void drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation);
		void drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation);
		void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation);
		void drawAuto();
		void drawIndexedInstancedIndirect(const rend::BufferResource* argsBuffer, uint32_t alignedByteOffsetForArgs);
		void drawInstancedIndirect(const rend::BufferResource* argsBuffer, uint32_t alignedByteOffsetForArgs);

		// Constant Buffers
		void setConstantBuffers(rend::ShaderStage stage, uint32_t startSlot, uint32_t count, const rend::ConstantBuffer* const* constantBuffers);

		template <rend::ShaderStage Stage, typename... Buffers, typename = std::enable_if_t<(std::is_convertible_v<Buffers, const rend::ConstantBuffer*> && ...)>>
		void setConstantBuffers(uint32_t startSlot, const Buffers&... buffers) {
			static_assert(sizeof...(Buffers) > 0, "at least one buffer required");
			static_assert((std::is_convertible_v<Buffers, const rend::ConstantBuffer*> && ...), "wrong buffer type");

			const rend::ConstantBuffer* array[] = { buffers... };
			this->setConstantBuffers(Stage, startSlot, static_cast<uint32_t>(sizeof...(Buffers)), array);
		}

		template <rend::ShaderStage Stage, typename First, typename... Rest, typename = std::enable_if_t<(std::is_convertible_v<First, const rend::ConstantBuffer*> && (std::is_convertible_v<Rest, const rend::ConstantBuffer*> && ...))>>
		void setConstantBuffers(const First& first, const Rest&... rest) {
			this->setConstantBuffers<Stage>(static_cast<uint32_t>(0), first, rest...);
		}

		template <rend::ShaderStage Stage, typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::ConstantBuffer*>>>
		void setConstantBuffers(uint32_t startSlot, const std::vector<Ptr>& buffers) {
			this->setConstantBuffers(Stage, startSlot, static_cast<uint32_t>(buffers.size()), buffers.data());
		}

		template <rend::ShaderStage Stage, typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::ConstantBuffer*>>>
		void setConstantBuffers(const std::vector<Ptr>& buffers) {
			this->setConstantBuffers<Stage>(static_cast<uint32_t>(0), buffers);
		}

		template <rend::ShaderStage Stage>
		void clearConstantBuffers() {
			this->setConstantBuffers(Stage, static_cast<uint32_t>(0), static_cast<uint32_t>(0), static_cast<const rend::ConstantBuffer* const*>(nullptr));
		}

		// Shader Resource Views
		void setShaderResourceViews(rend::ShaderStage stage, uint32_t startSlot, uint32_t count, const rend::ShaderResourceView* const* shaderResourceViews);

		template <rend::ShaderStage Stage, typename... Resources, typename = std::enable_if_t<(std::is_convertible_v<Resources, const rend::ShaderResourceView*> && ...)>>
		void setShaderResourceViews(uint32_t startSlot, const Resources&... resources) {
			static_assert(sizeof...(Resources) > 0, "at least one resource required");
			static_assert((std::is_convertible_v<Resources, const rend::ShaderResourceView*> && ...), "wrong resource type");

			const rend::ShaderResourceView* array[] = { resources... };
			this->setShaderResourceViews(Stage, startSlot, static_cast<uint32_t>(sizeof...(Resources)), array);
		}

		template <rend::ShaderStage Stage, typename First, typename... Rest, typename = std::enable_if_t<(std::is_convertible_v<First, const rend::ShaderResourceView*> && (std::is_convertible_v<Rest, const rend::ShaderResourceView*> && ...))>>
		void setShaderResourceViews(const First& first, const Rest&... rest) {
			this->setShaderResourceViews<Stage>(static_cast<uint32_t>(0), first, rest...);
		}

		template <rend::ShaderStage Stage, typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::ShaderResourceView*>>>
		void setShaderResourceViews(uint32_t startSlot, const std::vector<Ptr>& resources) {
			this->setShaderResourceViews(Stage, startSlot, static_cast<uint32_t>(resources.size()), resources.data());
		}

		template <rend::ShaderStage Stage, typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::ShaderResourceView*>>>
		void setShaderResourceViews(const std::vector<Ptr>& resources) {
			this->setShaderResourceViews<Stage>(static_cast<uint32_t>(0), resources);
		}

		template <rend::ShaderStage Stage>
		void clearShaderResourceViews() {
			this->setShaderResourceViews(Stage, static_cast<uint32_t>(0), static_cast<uint32_t>(0), static_cast<const rend::ShaderResourceView* const*>(nullptr));
		}

		// Sampler States
		void setSamplerStates(rend::ShaderStage stage, uint32_t startSlot, uint32_t count, const rend::SamplerState* const* samplerStates);

		template <rend::ShaderStage Stage, typename... Samplers, typename = std::enable_if_t<(std::is_convertible_v<Samplers, const rend::SamplerState*> && ...)>>
		void setSamplerStates(uint32_t startSlot, const Samplers&... samplers) {
			static_assert(sizeof...(Samplers) > 0, "at least one sampler required");
			static_assert((std::is_convertible_v<Samplers, const rend::SamplerState*> && ...), "wrong sampler type");

			const rend::SamplerState* array[] = { samplers... };
			this->setSamplerStates(Stage, startSlot, static_cast<uint32_t>(sizeof...(Samplers)), array);
		}

		template <rend::ShaderStage Stage, typename First, typename... Rest, typename = std::enable_if_t<(std::is_convertible_v<First, const rend::SamplerState*> && (std::is_convertible_v<Rest, const rend::SamplerState*> && ...))>>
		void setSamplerStates(const First& first, const Rest&... rest) {
			this->setSamplerStates<Stage>(static_cast<uint32_t>(0), first, rest...);
		}

		template <rend::ShaderStage Stage, typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::SamplerState*>>>
		void setSamplerStates(uint32_t startSlot, const std::vector<Ptr>& samplers) {
			this->setSamplerStates(Stage, startSlot, static_cast<uint32_t>(samplers.size()), samplers.data());
		}

		template <rend::ShaderStage Stage, typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::SamplerState*>>>
		void setSamplerStates(const std::vector<Ptr>& samplers) {
			this->setSamplerStates<Stage>(static_cast<uint32_t>(0), samplers);
		}

		template <rend::ShaderStage Stage>
		void clearSamplerStates() {
			this->setSamplerStates(Stage, static_cast<uint32_t>(0), static_cast<uint32_t>(0), static_cast<const rend::SamplerState* const*>(nullptr));
		}

		// Vertex buffers
		void setVertexBuffers(uint32_t startSlot, uint32_t count, const rend::VertexBuffer* const* vertexBuffers);

		template <typename... Buffers, typename = std::enable_if_t<(std::is_convertible_v<Buffers, const rend::VertexBuffer*> && ...)>>
		void setVertexBuffers(uint32_t startSlot, const Buffers&... buffers) {
			static_assert(sizeof...(Buffers) > 0, "at least one buffer required");
			static_assert((std::is_convertible_v<Buffers, const rend::VertexBuffer*> && ...), "wrong buffer type");

			const rend::VertexBuffer* array[] = { buffers... };
			this->setVertexBuffers(startSlot, static_cast<uint32_t>(sizeof...(Buffers)), array);
		}

		template <typename First, typename... Rest, typename = std::enable_if_t<(std::is_convertible_v<First, const rend::VertexBuffer*> && (std::is_convertible_v<Rest, const rend::VertexBuffer*> && ...))>>
		void setVertexBuffers(const First& first, const Rest&... rest) {
			this->setVertexBuffers(static_cast<uint32_t>(0), first, rest...);
		}

		template <typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::VertexBuffer*>>>
		void setVertexBuffers(uint32_t startSlot, const std::vector<Ptr>& buffers) {
			this->setVertexBuffers(startSlot, static_cast<uint32_t>(buffers.size()), buffers.data());
		}

		template <typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::VertexBuffer*>>>
		void setVertexBuffers(const std::vector<Ptr>& buffers) {
			this->setVertexBuffers(static_cast<uint32_t>(0), buffers);
		}

		void clearVertexBuffers() {
			this->setVertexBuffers(static_cast<uint32_t>(0), static_cast<uint32_t>(0), static_cast<const rend::VertexBuffer* const*>(nullptr));
		}

		// Render Views
		void setRenderViews(uint32_t startSlot, uint32_t count, const rend::RenderView* const* renderViews);

		template <typename... Views, typename = std::enable_if_t<(std::is_convertible_v<Views, const rend::RenderView*> && ...)>>
		void setRenderViews(uint32_t startSlot, const Views&... views) {
			static_assert(sizeof...(Views) > 0, "at least one view required");
			static_assert((std::is_convertible_v<Views, const rend::RenderView*> && ...), "wrong view type");

			const rend::RenderView* array[] = { views... };
			this->setRenderViews(startSlot, static_cast<uint32_t>(sizeof...(Views)), array);
		}

		template <typename First, typename... Rest, typename = std::enable_if_t<(std::is_convertible_v<First, const rend::RenderView*> && (std::is_convertible_v<Rest, const rend::RenderView*> && ...))>>
		void setRenderViews(const First& first, const Rest&... rest) {
			this->setRenderViews(static_cast<uint32_t>(0), first, rest...);
		}

		template <typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::RenderView*>>>
		void setRenderViews(uint32_t startSlot, const std::vector<Ptr>& views) {
			this->setRenderViews(startSlot, static_cast<uint32_t>(views.size()), views.data());
		}

		template <typename Ptr, typename = std::enable_if_t<std::is_convertible_v<Ptr, const rend::RenderView*>>>
		void setRenderViews(const std::vector<Ptr>& views) {
			this->setRenderViews(static_cast<uint32_t>(0), views);
		}

		void clearRenderViews() {
			this->setRenderViews(static_cast<uint32_t>(0), static_cast<uint32_t>(0), static_cast<const rend::RenderView* const*>(nullptr));
		}

		// Scissor Rects
		void setScissorRects(uint32_t count, const rend::Rectangle* scissorRects);

		template <typename... Rects, typename = std::enable_if_t<(std::is_convertible_v<Rects, rend::Rectangle> && ...)>>
		void setScissorRects(const Rects&... rects) {
			static_assert(sizeof...(Rects) > 0, "at least one rect required");

			const rend::Rectangle array[] = { rects... };
			this->setScissorRects(static_cast<uint32_t>(sizeof...(Rects)), array);
		}

		void setScissorRects(const std::vector<rend::Rectangle>& rects) {
			this->setScissorRects(static_cast<uint32_t>(rects.size()), rects.data());
		}

		void clearScissorRects() {
			this->setScissorRects(static_cast<uint32_t>(0), static_cast<const rend::Rectangle*>(nullptr));
		}

		// Viewports
		void setViewports(uint32_t count, const rend::Viewport* viewports);

		template <typename... Viewports, typename = std::enable_if_t<(std::is_convertible_v<Viewports, rend::Viewport> && ...)>>
		void setViewports(const Viewports&... viewports) {
			static_assert(sizeof...(Viewports) > 0, "at least one viewport required");

			const rend::Viewport array[] = { viewports... };
			this->setViewports(static_cast<uint32_t>(sizeof...(Viewports)), array);
		}

		void setViewports(const std::vector<rend::Viewport>& viewports) {
			this->setViewports(static_cast<uint32_t>(viewports.size()), viewports.data());
		}

		void clearViewports() {
			this->setViewports(static_cast<uint32_t>(0), static_cast<const rend::Viewport*>(nullptr));
		}
	};

	typedef rend::RefCountPtr<rend::PipelineContext> PipelineContextHandle;
}