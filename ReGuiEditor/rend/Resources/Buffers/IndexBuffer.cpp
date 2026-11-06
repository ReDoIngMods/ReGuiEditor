#include "IndexBuffer.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <cstdint>
#include <vector>

inline static void UpdateBufferDescriptionIB(D3D11_BUFFER_DESC& bufferDesc, bool allowRead, bool allowWrite) {
	if (allowRead && allowWrite) {
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		bufferDesc.BindFlags = 0;
	} else if (allowRead) {
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.BindFlags = 0;
	} else if (allowWrite) {
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	} else {
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	}
}

bool rend::IndexBuffer::init(rend::IndexBufferFormat format, uint32_t indexCount, const void* initialData, bool allowRead, bool allowWrite) {
	if (indexCount == 0)
		indexCount = 1;

	uint32_t strideSize = rend::getIndexBufferFormatSize(format);
	if (strideSize == 0) {
		CONSOLE_ERROR("Invalid index buffer format");
		return false;
	}

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	UpdateBufferDescriptionIB(bufferDesc, allowRead, allowWrite);
	bufferDesc.ByteWidth = strideSize * indexCount;

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
		CONSOLE_ERROR("Failed to create index buffer! HRESULT: 0x%08X", result);
		return false;
	}

	this->bufferSize_ = bufferDesc.ByteWidth;
	this->indexCount_ = indexCount;
	this->format_ = format;

	this->allowReadAccess_ = allowRead;
	this->allowWriteAccess_ = allowWrite;

	return true;
}

bool rend::IndexBuffer::resizeBuffer(uint32_t newCount, bool copyOldData, bool forceShrink) {
	if (newCount == 0)
		newCount = 1;

	if (!forceShrink && newCount <= this->indexCount_)
		return true;

	std::vector<BYTE> oldData;
	if (copyOldData && this->buffer_) {
		oldData.resize(this->bufferSize_);
	
		if (!this->readRawBuffer(oldData.data(), this->bufferSize_)) {
			CONSOLE_ERROR("Failed to read old data from index buffer for resizing");
			return false;
		}
	}

	uint32_t strideSize = rend::getIndexBufferFormatSize(this->format_);
	if (strideSize == 0) {
		CONSOLE_ERROR("Invalid index buffer format during resize");
		return false;
	}

	this->buffer_.reset();

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	UpdateBufferDescriptionIB(bufferDesc, this->allowReadAccess_, this->allowWriteAccess_);
	bufferDesc.ByteWidth = strideSize * newCount;

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
		CONSOLE_ERROR("Failed to create index buffer! HRESULT: 0x%08X", result);
		return false;
	}

	this->bufferSize_ = bufferDesc.ByteWidth;
	this->indexCount_ = newCount;
	return true;
}

bool rend::IndexBuffer::changeFormat(rend::IndexBufferFormat newFormat, bool copyOldData) {
	if (newFormat == this->format_) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "New format is the same as the current format. No changes made.");
		return true;
	}

	uint32_t newStrideSize = rend::getIndexBufferFormatSize(newFormat);
	if (newStrideSize == 0) {
		CONSOLE_ERROR("Invalid new index buffer format");
		return false;
	}
	
	std::vector<BYTE> updatedData;
	if (copyOldData && this->buffer_) {
		std::vector<BYTE> oldData(this->bufferSize_);

		if (!this->readRawBuffer(oldData.data(), this->bufferSize_)) {
			CONSOLE_ERROR("Failed to read old data from index buffer for format change");
			return false;
		}

		updatedData.resize(static_cast<size_t>(newStrideSize) * this->indexCount_);

		switch (newFormat) {
		case rend::IndexBufferFormat::UInt16:
			updatedData.resize(this->indexCount_ * sizeof(uint16_t));
			for (uint32_t index = 0; index < this->indexCount_; ++index) {
				uint32_t value = reinterpret_cast<uint32_t*>(oldData.data())[index];
				reinterpret_cast<uint16_t*>(updatedData.data())[index] = static_cast<uint16_t>(value);
			}

			break;
		case rend::IndexBufferFormat::UInt32:
			updatedData.resize(this->indexCount_ * sizeof(uint32_t));
			for (uint32_t index = 0; index < this->indexCount_; ++index) {
				uint16_t value = reinterpret_cast<uint16_t*>(oldData.data())[index];
				reinterpret_cast<uint32_t*>(updatedData.data())[index] = static_cast<uint32_t>(value);
			}

			break;
		default:
			CONSOLE_ERROR("Unsupported index buffer format change");
			return false;
		}
	}

	this->buffer_.reset();

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	UpdateBufferDescriptionIB(bufferDesc, this->allowReadAccess_, this->allowWriteAccess_);
	bufferDesc.ByteWidth = newStrideSize * this->indexCount_;

	HRESULT result = S_OK;
	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	if (copyOldData && !updatedData.empty()) {
		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(initData));
		initData.pSysMem = updatedData.data();
		
		result = device->CreateBuffer(&bufferDesc, &initData, &this->buffer_);
	} else {
		result = device->CreateBuffer(&bufferDesc, nullptr, &this->buffer_);
	}

	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to recreate index buffer for format change! HRESULT: 0x%08X", result);
		return false;
	}

	this->bufferSize_ = bufferDesc.ByteWidth;
	this->format_ = newFormat;
	return true;
}