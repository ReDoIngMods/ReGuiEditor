#pragma once
#include <rend/Common/RefCounter.hpp>
#include <rend/Common/Viewport.hpp>

#include <rend/Resources/BlendState.hpp>
#include <rend/Resources/PipelineContext.hpp>
#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/SamplerState.hpp>
#include <rend/Resources/ShaderResourceView.hpp>

#include <rend/Resources/Buffers/ConstantBuffer.hpp>

#include <rend/Resources/Shaders/PixelShader.hpp>
#include <rend/Resources/Shaders/VertexShader.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace rend {
	struct FullscreenPassDesc {
		// Pixel shader that does the actual work. Only SV_Position is guaranteed to be available as an input, which
		// is all that's needed for anything procedural (grids, gradients, ...). Anything else the vertex shader
		// outputs (e.g. UVs) can be declared as well.
		std::string pixelShader;
		std::string pixelEntrypoint = "psMain";
		std::vector<std::string> macros;

		// Vertex shader that generates the triangle from SV_VertexID. Shared by every pass, so there's normally no
		// reason to change this.
		std::string vertexShader = "$RGE_DATA/Shaders/gui_fullscreen_triangle.hlsl";
		std::string vertexEntrypoint = "vsMain";

		// If non-zero, the pass owns a constant buffer of this size that's bound to pixel shader slot b0 and filled from
		// FullscreenDrawParams::constants on every draw. Must be a multiple of 16 (use alignas(16) on the struct).
		uint32_t constantBufferSize = 0;

		std::string debugName;
	};

	struct FullscreenDrawParams {
		// Where to draw. Must stay alive for the duration of the call.
		rend::RenderView* target = nullptr;

		// Region of the target that gets drawn to. The triangle covers exactly this region, anything outside is untouched.
		// Note that SV_Position in the shader is still in *target* pixels, not relative to this viewport.
		rend::Viewport viewport;

		// Copied into the pass's own constant buffer (FullscreenPassDesc::constantBufferSize bytes) before drawing.
		const void* constants = nullptr;

		// Bound starting at slot 0 of the pixel stage. Typically the input for post-processing effects.
		std::vector<const rend::ShaderResourceView*> shaderResourceViews;
		std::vector<const rend::SamplerState*> samplerStates;

		// Bound after the pass's own constant buffer (if it has one), so these start at b1 in that case.
		std::vector<const rend::ConstantBuffer*> extraConstantBuffers;

		// nullptr = opaque overwrite.
		const rend::BlendState* blendState = nullptr;
	};

	// Draws a single oversized triangle that covers a whole viewport with a user supplied pixel shader. This is the
	// cheapest way to run a pixel shader over every pixel of a region: post-processing, blits, procedural backgrounds.
	//
	// Typical use:
	//   rend::FullscreenPassDesc desc;
	//   desc.pixelShader = "$RGE_DATA/Shaders/my_effect.hlsl";
	//   desc.constantBufferSize = sizeof(MyConstants);
	//   pass.init(desc);
	//
	//   rend::FullscreenDrawParams params;
	//   params.target = renderView;
	//   params.viewport = rend::Viewport(0, 0, width, height, 0, 1);
	//   params.constants = &myConstants;
	//   params.shaderResourceViews = { inputTexture };
	//   pass.draw(params);
	//
	// Every pass has its own PipelineContext, so draw() can safely be called while another context is active (for
	// example from inside an ImGui draw callback).
	class FullscreenPass {
		rend::VertexShaderHandle vertexShader_;
		rend::PixelShaderHandle pixelShader_;
		rend::ConstantBufferHandle constantBuffer_;
		rend::PipelineContextHandle pipelineContext_;

		uint32_t constantBufferSize_ = 0;
	public:
		FullscreenPass() = default;
		~FullscreenPass() { this->cleanup(); }

		FullscreenPass(const FullscreenPass&) = delete;
		FullscreenPass& operator=(const FullscreenPass&) = delete;

		[[nodiscard]] bool init(const FullscreenPassDesc& desc);
		void cleanup();

		[[nodiscard]] bool isValid() const { return this->pipelineContext_ && this->pixelShader_ && this->vertexShader_; }

		bool draw(const FullscreenDrawParams& params);
	};
}