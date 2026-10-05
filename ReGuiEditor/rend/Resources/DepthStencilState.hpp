#pragma once
#include "Resource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/Common/ComparisonFunction.hpp>
#include <rend/Common/StencilOperand.hpp>

#include <d3d11.h>

#include <cstdint>

namespace rend {
	struct DepthStencilOpDesc {
		rend::StencilOperand stencilFailOp = rend::StencilOperand::Keep;
		rend::StencilOperand stencilDepthFailOp = rend::StencilOperand::Keep;
		rend::StencilOperand stencilPassOp = rend::StencilOperand::Keep;
		rend::ComparisonFunction stencilFunc = rend::ComparisonFunction::Always;
	};

	struct DepthStencilDesc {
		bool depthEnable = true;
		bool depthWriteMask = true;
		rend::ComparisonFunction depthFunc = rend::ComparisonFunction::Less;
		bool stencilEnable = false;
		uint8_t stencilReadMask = 0xFF;
		uint8_t stencilWriteMask = 0xFF;
		rend::DepthStencilOpDesc frontFace;
		rend::DepthStencilOpDesc backFace;
	};

	class DepthStencilState : public rend::RefCounter<rend::Resource> {
		rend::RefCountPtr<ID3D11DepthStencilState> depthStencilState_ = nullptr;
		uint32_t stencilRef_ = 0;

		rend::DepthStencilDesc desc_;
	public:
		[[nodiscard]] bool init(const rend::DepthStencilDesc& desc);

		[[nodiscard]] uint32_t getStencilRef() const { return this->stencilRef_; }
		void setStencilRef(uint32_t stencilRef) { this->stencilRef_ = stencilRef; }

		[[nodiscard]] rend::DepthStencilDesc getDesc() const { return this->desc_; }

		[[nodiscard]] rend::RefCountPtr<ID3D11DepthStencilState> getDepthStencilState() const { return this->depthStencilState_; }
	};

	typedef rend::RefCountPtr<rend::DepthStencilState> DepthStencilStateHandle;
}