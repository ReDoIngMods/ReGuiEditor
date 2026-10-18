#include "VertexBuffer.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <cstdint>
#include <vector>


inline static void UpdateBufferDescriptionVS(D3D11_BUFFER_DESC& desc, bool allowRead, bool allowWrite) {
	if (allowRead && allowWrite) {
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = 0;
	} else if (allowRead) {
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
	} else if (allowWrite) {
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	} else {
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	}
}

bool rend::VertexBuffer::init(uint32_t vertexSize, uint32_t vertexCount, const void* initialData, bool allowRead, bool allowWrite) {
	if (vertexSize == 0) {
		CONSOLE_ERROR("Vertex size must be greater than zero");
		return false;
	}

	if (vertexCount == 0)
		vertexCount = 1;

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	UpdateBufferDescriptionVS(bufferDesc, allowRead, allowWrite);
	bufferDesc.ByteWidth = vertexSize * vertexCount;

	HRESULT result = S_OK;

	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	if (initialData) {
		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(initData));
		initData.pSysMem = initialData;
	
		result = device->CreateBuffer(&bufferDesc, &initData, &this->buffer_);
	} else {
		result = device->CreateBuffer(&bufferDesc, nullptr, &this->buffer_);
	}

	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create vertex buffer! HRESULT: 0x%08X", result);
		return false;
	}

	this->bufferSize_ = bufferDesc.ByteWidth;
	this->vertexSize_ = vertexSize;
	this->elementCount_ = vertexCount;
	
	this->allowReadAccess_ = allowRead;
	this->allowWriteAccess_ = allowWrite;

	return true;
}

bool rend::VertexBuffer::resizeBuffer(uint32_t newCount, bool copyOldData) {
	if (newCount == 0) {
		CONSOLE_ERROR("New count must be greater than zero");
		return false;
	}

	if (newCount == this->elementCount_)
		return true;

	std::vector<uint8_t> oldData;
	if (copyOldData) {
		oldData.resize(this->bufferSize_);
	
		if (!this->readRawBuffer(oldData.data(), this->bufferSize_)) {
			CONSOLE_ERROR("Failed to read old buffer data for resizing");
			return false;
		}
	}

	this->buffer_.reset();

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	UpdateBufferDescriptionVS(bufferDesc, this->allowReadAccess_, this->allowWriteAccess_);
	bufferDesc.ByteWidth = this->vertexSize_ * newCount;

	if (copyOldData)
		oldData.resize(bufferDesc.ByteWidth);

	HRESULT result = S_OK;
	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	if (copyOldData && !oldData.empty()) {
		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(initData));
		initData.pSysMem = oldData.data();

		result = device->CreateBuffer(&bufferDesc, &initData, &this->buffer_);
	} else {
		result = device->CreateBuffer(&bufferDesc, nullptr, &this->buffer_);
	}

	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to resize vertex buffer! HRESULT: 0x%08X", result);
		return false;
	}

	this->bufferSize_ = bufferDesc.ByteWidth;
	this->elementCount_ = newCount;

	return true;
}