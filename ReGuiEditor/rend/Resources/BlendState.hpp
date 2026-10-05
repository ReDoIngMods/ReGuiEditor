#pragma once
#include "Resource.hpp"

#include <rend/Common/BlendMode.hpp>
#include <rend/Common/BlendOperand.hpp>
#include <rend/Common/Color.hpp>
#include <rend/Common/RefCounter.hpp>

#include <d3d11.h>

#include <array>
#include <cstdint>

namespace rend {
	enum class ColorWriteMask : uint8_t {
		Red = 1, // D3D11_COLOR_WRITE_ENABLE_RED
		Green = 2, // D3D11_COLOR_WRITE_ENABLE_GREEN
		Blue = 4, // D3D11_COLOR_WRITE_ENABLE_BLUE
		Alpha = 8, // D3D11_COLOR_WRITE_ENABLE_ALPHA
		All = 15
	};

	inline rend::ColorWriteMask operator|(rend::ColorWriteMask a, rend::ColorWriteMask b) {
		return static_cast<rend::ColorWriteMask>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	inline rend::ColorWriteMask operator&(rend::ColorWriteMask a, rend::ColorWriteMask b) {
		return static_cast<rend::ColorWriteMask>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}

	struct BlendDesc {
		bool alphaToCoverage = false;
		bool independentBlend = false;

		struct RenderTargetBlendDesc {
			bool blendEnable = false;
			rend::BlendMode srcBlend = rend::BlendMode::Zero;
			rend::BlendMode destBlend = rend::BlendMode::Zero;
			rend::BlendOperand blendOperand = rend::BlendOperand::Add;

			bool alphaBlendEnable = false;
			rend::BlendMode srcAlphaBlend = rend::BlendMode::Zero;
			rend::BlendMode destAlphaBlend = rend::BlendMode::Zero;
			rend::BlendOperand alphaBlendOperand = rend::BlendOperand::Add;

			rend::ColorWriteMask renderTargetWriteMask = rend::ColorWriteMask::All;
		};

		std::array<rend::BlendDesc::RenderTargetBlendDesc, 8> renderTargets;
	};

	class BlendState : public rend::RefCounter<rend::Resource> {
		rend::RefCountPtr<ID3D11BlendState> blendState_ = nullptr;

		rend::BlendDesc desc_;
		
		rend::Color blendFactor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t sampleMask_ = 0xFFFFFFFF;
	public:
		[[nodiscard]] bool init(const rend::BlendDesc& desc);

		[[nodiscard]] rend::Color getBlendFactor() const { return this->blendFactor_; }
		[[nodiscard]] uint32_t getSampleMask() const { return this->sampleMask_; }

		void setBlendFactor(const rend::Color& factor) { this->blendFactor_ = factor; }
		void setSampleMask(uint32_t mask) { this->sampleMask_ = mask; }

		[[nodiscard]] rend::BlendDesc getDesc() const { return this->desc_; }

		[[nodiscard]] rend::RefCountPtr<ID3D11BlendState> getBlendState() const { return this->blendState_; }
	};

	typedef rend::RefCountPtr<rend::BlendState> BlendStateHandle;
}