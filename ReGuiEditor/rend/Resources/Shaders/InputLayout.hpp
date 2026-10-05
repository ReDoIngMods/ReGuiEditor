#pragma once
#include <rend/Common/Format.hpp>
#include <rend/Common/RefCounter.hpp>

#include <rend/Resources/Resource.hpp>

#include "VertexShader.hpp"

#include <d3d11.h>
#include <dxgiformat.h>

#include <cstdint>
#include <cstring>
#include <vector>

#define REND_APPEND_ALIGNED_ELEMENT 0xFFFFFFFF

namespace rend {
	enum class InputClassification : uint8_t {
		PerVertexData = 0,  // D3D11_INPUT_PER_VERTEX_DATA
		PerInstanceData = 1 // D3D11_INPUT_PER_INSTANCE_DATA
	};

	struct InputLayoutDesc {
	public:
		std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDescs;

		void addElement(const char* semanticName, uint32_t semanticIndex, rend::Format format, uint32_t inputSlot = 0, uint32_t alignedByteOffset = REND_APPEND_ALIGNED_ELEMENT, rend::InputClassification inputSlotClass = rend::InputClassification::PerVertexData, uint32_t instanceDataStepRate = 0) {
			D3D11_INPUT_ELEMENT_DESC d3dElementDesc;
			memset(&d3dElementDesc, 0, sizeof(D3D11_INPUT_ELEMENT_DESC));
			d3dElementDesc.SemanticName = semanticName;
			d3dElementDesc.SemanticIndex = semanticIndex;
			d3dElementDesc.Format = static_cast<DXGI_FORMAT>(format);
			d3dElementDesc.InputSlot = inputSlot;
			d3dElementDesc.AlignedByteOffset = alignedByteOffset;
			d3dElementDesc.InputSlotClass = static_cast<D3D11_INPUT_CLASSIFICATION>(inputSlotClass);
			d3dElementDesc.InstanceDataStepRate = instanceDataStepRate;

			this->inputElementDescs.push_back(d3dElementDesc);
		}
	};

	class InputLayout : public rend::RefCounter<rend::Resource> {
		rend::RefCountPtr<ID3D11InputLayout> inputLayout_;
		rend::VertexShaderHandle vertexShader_;

		rend::InputLayoutDesc inputLayoutDesc_;
	public:
		[[nodiscard]] bool init(const rend::InputLayoutDesc& inputElementDescs, rend::VertexShader* vertexShader);

		[[nodiscard]] rend::RefCountPtr<ID3D11InputLayout> getInputLayout() const { return this->inputLayout_; }
		[[nodiscard]] rend::InputLayoutDesc getInputLayoutDesc() const { return this->inputLayoutDesc_; }
	};

	typedef rend::RefCountPtr<rend::InputLayout> InputLayoutHandle;
}