#include "BufferResource.hpp"

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>

#include <cstdint>
#include <cstring>

rend::BufferResource::~BufferResource() {
	if (this->isMapped_) {
		CONSOLE_WARNING("Buffer is still mapped! You fucked something up mate... fix yo shit dumbass");
		rend::D3D11Device::getInstance().getDeviceContext1()->Unmap(this->buffer_, 0);
	}
}

uint32_t rend::BufferResource::getBufferSize() const {
	return this->bufferSize_;
}

bool rend::BufferResource::mapBuffer(void** data, rend::BufferMapType mapType) {
	if (!this->buffer_) {
		CONSOLE_ERROR("Cannot map an uninitialised buffer");
		return false;
	}

	if (!data) {
		CONSOLE_ERROR("Data pointer is null");
		return false;
	}

	if (this->isMapped_) {
		CONSOLE_ERROR("Buffer is already mapped");
		return false;
	}

	D3D11_MAP d3dMapType;
	switch (mapType) {
	case rend::BufferMapType::Read:
		d3dMapType = D3D11_MAP_READ;

		if (!this->allowReadAccess_) {
			CONSOLE_ERROR("Buffer was not created with read access");
			return false;
		}

		break;
	case rend::BufferMapType::Write:
		d3dMapType = D3D11_MAP_WRITE;

		if (!this->allowWriteAccess_) {
			CONSOLE_ERROR("Buffer was not created with write access");
			return false;
		}

		break;
	case rend::BufferMapType::ReadWrite:
		d3dMapType = D3D11_MAP_READ_WRITE;

		if (!this->allowReadAccess_ || !this->allowWriteAccess_) {
			CONSOLE_ERROR("Buffer was not created with read/write access");
			return false;
		}

		break;
	case rend::BufferMapType::WriteDiscard:
		d3dMapType = D3D11_MAP_WRITE_DISCARD;

		if (!this->allowWriteAccess_) {
			CONSOLE_ERROR("Buffer was not created with write access");
			return false;
		}

		break;
	case rend::BufferMapType::WriteNoOverwrite:
		d3dMapType = D3D11_MAP_WRITE_NO_OVERWRITE;

		if (!this->allowWriteAccess_) {
			CONSOLE_ERROR("Buffer was not created with write access");
			return false;
		}

		break;
	default:
		CONSOLE_ERROR("Invalid BufferMapType specified");
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = rend::D3D11Device::getInstance().getDeviceContext1()->Map(this->buffer_, 0, d3dMapType, 0, &mappedResource);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to map buffer resource! HRESULT: 0x%08X", result);
		return false;
	}

	this->isMapped_ = true;

	*data = mappedResource.pData;
	return true;
}

bool rend::BufferResource::unmapBuffer() {
	if (!this->buffer_) {
		CONSOLE_ERROR("Cannot unmap an uninitialised buffer");
		return false;
	}

	if (!this->isMapped_) {
		CONSOLE_ERROR("Buffer is not mapped");
		return false;
	}

	rend::D3D11Device::getInstance().getDeviceContext1()->Unmap(this->buffer_, 0);
	
	this->isMapped_ = false;
	return true;
}

bool rend::BufferResource::readRawBuffer(void* destination, uint32_t size) {
	if (!this->buffer_) {
		CONSOLE_ERROR("Cannot read from an uninitialised buffer");
		return false;
	}

	if (!destination) {
		CONSOLE_ERROR("Destination pointer is null");
		return false;
	}

	if (size > this->getBufferSize()) {
		CONSOLE_ERROR("Read size exceeds buffer size");
		return false;
	}

	void* data = nullptr;
	if (!this->mapBuffer(&data, rend::BufferMapType::Read)) {
		CONSOLE_ERROR("Failed to map buffer for reading");
		return false;
	}

	memcpy(destination, data, size);

	if (!this->unmapBuffer()) {
		CONSOLE_ERROR("Failed to unmap buffer after reading");
		return false;
	}

	return true;
}

bool rend::BufferResource::writeRawBuffer(const void* source, uint32_t size) {
	if (!this->buffer_) {
		CONSOLE_ERROR("Cannot write to an uninitialised buffer");
		return false;
	}

	if (!source) {
		CONSOLE_ERROR("Source pointer is null");
		return false;
	}

	if (size > this->getBufferSize()) {
		CONSOLE_ERROR("Write size exceeds buffer size");
		return false;
	}

	void* data = nullptr;
	if (!this->mapBuffer(&data, rend::BufferMapType::WriteDiscard)) {
		CONSOLE_ERROR("Failed to map buffer for writing");
		return false;
	}

	memcpy(data, source, size);

	if (!this->unmapBuffer()) {
		CONSOLE_ERROR("Failed to unmap buffer after writing");
		return false;
	}

	return true;
}