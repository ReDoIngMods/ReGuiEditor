#include "ConstantBuffer.hpp"

#include <rend/D3D11Device.hpp>

#include <rend/Common/Alignment.hpp>
#include <rend/Common/RefCounter.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <cstdint>
#include <cstring>
#include <vector>

bool rend::ConstantBuffer::init(uint32_t dataSize, const void* initialData) {
	if (dataSize == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Data size must be greater than zero");
		return false;
	}

	UINT alignedSize = rend::alignTo(dataSize, 16u);

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = alignedSize;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	
	HRESULT result = S_OK;
	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	if (initialData) {
		std::vector<uint8_t> paddedData(alignedSize, 0);
		memcpy(paddedData.data(), initialData, dataSize);

		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(initData));
		initData.pSysMem = paddedData.data();

		result = device->CreateBuffer(&bufferDesc, &initData, &this->buffer_);
	} else {
		result = device->CreateBuffer(&bufferDesc, nullptr, &this->buffer_);
	}

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create constant buffer! HRESULT: 0x%08X", result);
		return false;
	}

	this->bufferSize_ = alignedSize;

	this->allowWriteAccess_ = true;
	return true;
}
