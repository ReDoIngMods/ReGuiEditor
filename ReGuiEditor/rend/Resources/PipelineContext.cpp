#include "PipelineContext.hpp"

#include <rend/D3D11Device.hpp>

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
#include "SamplerState.hpp"

#include "RenderView.hpp"
#include "ShaderResourceView.hpp"

#include "Shaders/InputLayout.hpp"
#include "Shaders/PixelShader.hpp"
#include "Shaders/VertexShader.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

#include <cstdint>
#include <vector>

bool rend::PipelineContext::init() {
	rend::D3D11Device& renderer = rend::D3D11Device::getInstance();
	this->deviceContext1_ = renderer.getDeviceContext1();
	this->device1_ = renderer.getDevice1();

	D3D_FEATURE_LEVEL featureLevel = this->device1_->GetFeatureLevel();
	HRESULT result = this->device1_->CreateDeviceContextState(0, &featureLevel, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &this->deviceContextState_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create device context state! HRESULT: 0x%08X", result);
		return false;
	}

	return true;
}

bool rend::PipelineContext::begin() {
	if (this->isActive_) {
		CONSOLE_ERROR("Pipeline context already begun. Call end() before beginning again.");
		return false;
	}

	this->deviceContext1_->SwapDeviceContextState(this->deviceContextState_, this->previousDeviceContextState_.getAddressOf());

	this->isActive_ = true;
	return true;
}

bool rend::PipelineContext::end() {
	if (!this->isActive_) {
		CONSOLE_ERROR("Pipeline context not begun. Call begin() before ending.");
		return false;
	}

	this->deviceContext1_->SwapDeviceContextState(this->previousDeviceContextState_, nullptr);
	this->previousDeviceContextState_.reset();

	this->isActive_ = false;
	return true;
}

void rend::PipelineContext::setRasterizerState(const rend::RasterizerState* rasterizerState) {
	this->deviceContext1_->RSSetState(rasterizerState ? rasterizerState->getRasterizerState() : nullptr);
}

void rend::PipelineContext::setBlendState(const rend::BlendState* blendState) {
	if (blendState)
		this->deviceContext1_->OMSetBlendState(blendState->getBlendState(), blendState->getBlendFactor().components, blendState->getSampleMask());
	else
		this->deviceContext1_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
}

void rend::PipelineContext::setDepthStencilState(const rend::DepthStencilState* depthStencilState) {
	if (depthStencilState)
		this->deviceContext1_->OMSetDepthStencilState(depthStencilState->getDepthStencilState(), depthStencilState->getStencilRef());
	else
		this->deviceContext1_->OMSetDepthStencilState(nullptr, 0);
}

void rend::PipelineContext::setInputLayout(const rend::InputLayout* inputLayout) {
	this->deviceContext1_->IASetInputLayout(inputLayout ? inputLayout->getInputLayout() : nullptr);
}

void rend::PipelineContext::setVertexShader(const rend::VertexShader* vertexShader) {
	this->deviceContext1_->VSSetShader(vertexShader ? vertexShader->getShader() : nullptr, nullptr, 0);
}

void rend::PipelineContext::setPixelShader(const rend::PixelShader* pixelShader) {
	this->deviceContext1_->PSSetShader(pixelShader ? pixelShader->getShader() : nullptr, nullptr, 0);
}

void rend::PipelineContext::setIndexBuffer(const rend::IndexBuffer* indexBuffer) {
	if (indexBuffer)
		this->deviceContext1_->IASetIndexBuffer(indexBuffer->getBuffer(), static_cast<DXGI_FORMAT>(indexBuffer->getFormat()), 0);
	else
		this->deviceContext1_->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
}

void rend::PipelineContext::setPrimitiveTopology(rend::PrimitiveTopology topology) {
	this->deviceContext1_->IASetPrimitiveTopology(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topology));
}

void rend::PipelineContext::draw(uint32_t vertexCount, uint32_t startVertexLocation) {
	this->deviceContext1_->Draw(vertexCount, startVertexLocation);
}

void rend::PipelineContext::drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) {
	this->deviceContext1_->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
}

void rend::PipelineContext::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
	this->deviceContext1_->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void rend::PipelineContext::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
	this->deviceContext1_->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void rend::PipelineContext::drawAuto() {
	this->deviceContext1_->DrawAuto();
}

void rend::PipelineContext::drawIndexedInstancedIndirect(const rend::BufferResource* argsBuffer, uint32_t alignedByteOffsetForArgs) {
	this->deviceContext1_->DrawIndexedInstancedIndirect(argsBuffer->getBuffer(), alignedByteOffsetForArgs);
}

void rend::PipelineContext::drawInstancedIndirect(const rend::BufferResource* argsBuffer, uint32_t alignedByteOffsetForArgs) {
	this->deviceContext1_->DrawInstancedIndirect(argsBuffer->getBuffer(), alignedByteOffsetForArgs);
}

void rend::PipelineContext::setConstantBuffers(rend::ShaderStage stage, uint32_t startSlot, uint32_t count, const rend::ConstantBuffer* const* constantBuffers) {
	std::vector<ID3D11Buffer*> d3dBuffers(count);
	for (UINT index = 0; index < count; ++index)
		d3dBuffers[index] = constantBuffers[index] ? constantBuffers[index]->getBuffer() : nullptr;

	switch (stage) {
	case rend::ShaderStage::Vertex:	this->deviceContext1_->VSSetConstantBuffers(startSlot, count, d3dBuffers.data()); break;
	case rend::ShaderStage::Pixel:  this->deviceContext1_->PSSetConstantBuffers(startSlot, count, d3dBuffers.data()); break;
	default:
		CONSOLE_ERROR("Unsupported shader stage for setting constant buffers");
		return;
	}
}

void rend::PipelineContext::setShaderResourceViews(rend::ShaderStage stage, uint32_t startSlot, uint32_t count, const rend::ShaderResourceView* const* shaderResourceViews) {
	std::vector<ID3D11ShaderResourceView*> d3dSRVs(count);
	for (UINT index = 0; index < count; ++index)
		d3dSRVs[index] = shaderResourceViews[index] ? shaderResourceViews[index]->getShaderResourceView() : nullptr;

	switch (stage) {
	case rend::ShaderStage::Vertex:	this->deviceContext1_->VSSetShaderResources(startSlot, count, d3dSRVs.data()); break;
	case rend::ShaderStage::Pixel:	this->deviceContext1_->PSSetShaderResources(startSlot, count, d3dSRVs.data()); break;
	default:
		CONSOLE_ERROR("Unsupported shader stage for setting shader resource views");
		return;
	}
}

void rend::PipelineContext::setSamplerStates(rend::ShaderStage stage, uint32_t startSlot, uint32_t count, const rend::SamplerState* const* samplerStates) {
	std::vector<ID3D11SamplerState*> d3dSamplers(count);
	for (UINT index = 0; index < count; ++index)
		d3dSamplers[index] = samplerStates[index] ? samplerStates[index]->getSamplerState() : nullptr;

	switch (stage) {
	case rend::ShaderStage::Vertex:	this->deviceContext1_->VSSetSamplers(startSlot, count, d3dSamplers.data()); break;
	case rend::ShaderStage::Pixel:	this->deviceContext1_->PSSetSamplers(startSlot, count, d3dSamplers.data()); break;
	default:
		CONSOLE_ERROR("Unsupported shader stage for setting sampler states");
		return;
	}
}

void rend::PipelineContext::setVertexBuffers(uint32_t startSlot, uint32_t count, const rend::VertexBuffer* const* vertexBuffers) {
	std::vector<ID3D11Buffer*> d3dBuffers(count);
	std::vector<UINT> strides(count);
	std::vector<UINT> offsets(count);
	for (UINT index = 0; index < count; ++index) {
		const rend::VertexBuffer* vertexBuffer = vertexBuffers[index];
		if (vertexBuffer) {
			d3dBuffers[index] = vertexBuffer->getBuffer();
			strides[index] = vertexBuffer->getStride();
		} else {
			d3dBuffers[index] = nullptr;
			strides[index] = 0;
		}

		offsets[index] = 0; // TODO: This needs to be tested. ...and probably configurable too.
	}

	this->deviceContext1_->IASetVertexBuffers(startSlot, count, d3dBuffers.data(), strides.data(), offsets.data());
}

void rend::PipelineContext::setRenderViews(uint32_t startSlot, uint32_t count, const rend::RenderView* const* renderViews) {
	std::vector<ID3D11RenderTargetView*> d3dRTVs(count);
	for (UINT index = 0; index < count; ++index)
		d3dRTVs[index] = renderViews[index] ? renderViews[index]->getRenderTargetView() : nullptr;

	this->deviceContext1_->OMSetRenderTargets(count, d3dRTVs.data(), nullptr);
}

void rend::PipelineContext::setScissorRects(uint32_t count, const rend::Rectangle* scissorRects) {
	std::vector<D3D11_RECT> d3dRects(count);
	for (UINT index = 0; index < count; ++index) {
		const rend::Rectangle& rect = scissorRects[index];
		d3dRects[index].left = static_cast<LONG>(rect.left);
		d3dRects[index].top = static_cast<LONG>(rect.top);
		d3dRects[index].right = static_cast<LONG>(rect.right);
		d3dRects[index].bottom = static_cast<LONG>(rect.bottom);
	}

	this->deviceContext1_->RSSetScissorRects(count, d3dRects.data());
}

void rend::PipelineContext::setViewports(uint32_t count, const rend::Viewport* viewports) {
	std::vector<D3D11_VIEWPORT> d3dViewports(count);
	for (UINT index = 0; index < count; ++index) {
		const rend::Viewport& viewport = viewports[index];
		D3D11_VIEWPORT& d3dViewport = d3dViewports[index];

		d3dViewport.TopLeftX = viewport.x;
		d3dViewport.TopLeftY = viewport.y;
		d3dViewport.Width = viewport.width;
		d3dViewport.Height = viewport.height;
		d3dViewport.MinDepth = viewport.minDepth;
		d3dViewport.MaxDepth = viewport.maxDepth;
	}

	this->deviceContext1_->RSSetViewports(count, d3dViewports.data());
}