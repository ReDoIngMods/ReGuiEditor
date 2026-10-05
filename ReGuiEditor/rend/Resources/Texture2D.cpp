#include "Texture2D.hpp"

#include <rend/Common/Format.hpp>
#include <rend/Common/RefCounter.hpp>

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

#include <cstdint>
#include <cstring>

bool rend::Texture2D::init(uint32_t width, uint32_t height, rend::Format format, rend::Texture2DFlags flags) {
	if (width == 0 || height == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid texture dimensions: %ux%u", width, height);
		return false;
	}

	if (!(flags & rend::Texture2DFlags::ShaderResource) && !(flags & rend::Texture2DFlags::RenderTarget) && !(flags & rend::Texture2DFlags::StagingRead)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid texture flags: must specify at least one of ShaderResource, RenderTarget, or StagingRead");
		return false;
	}

	if ((flags & rend::Texture2DFlags::DynamicWrite) && (flags & rend::Texture2DFlags::StagingRead)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid texture flags: cannot specify both DynamicWrite and StagingRead");
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = static_cast<DXGI_FORMAT>(format);
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = 0;

	if (flags & rend::Texture2DFlags::DynamicWrite) {
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	if (flags & rend::Texture2DFlags::ShaderResource)
		desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

	if (flags & rend::Texture2DFlags::RenderTarget)
		desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

	if (flags & rend::Texture2DFlags::StagingRead) {
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
	}

	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	HRESULT result = device->CreateTexture2D(&desc, nullptr, this->texture_.getAddressOf());
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create texture! HRESULT: 0x%X", result);
		return false;
	}

	if (flags & rend::Texture2DFlags::ShaderResource) {
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = static_cast<DXGI_FORMAT>(format);
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		result = device->CreateShaderResourceView(this->texture_, &srvDesc, this->shaderResourceView_.getAddressOf());
		if (FAILED(result)) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ShaderResourceView for Texture2D! HRESULT: 0x%X", result);
			
			this->texture_.reset();
			return false;
		}
	}

	this->format_ = format;
	this->width_ = width;
	this->height_ = height;
	this->isShaderResourceViewSupported_ = (flags & rend::Texture2DFlags::ShaderResource) != 0;
	this->flags_ = flags;
	return true;
}

bool rend::Texture2D::initFromBackBuffer() {
	HRESULT result = rend::D3D11Device::getInstance().getSwapChain()->GetBuffer(0, IID_PPV_ARGS(&this->texture_));
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get back buffer texture! HRESULT: 0x%X", result);
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	this->texture_->GetDesc(&desc);

	this->width_ = desc.Width;
	this->height_ = desc.Height;
	this->format_ = static_cast<rend::Format>(desc.Format);

	if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = static_cast<DXGI_FORMAT>(this->format_);
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		
		if (FAILED(rend::D3D11Device::getInstance().getDevice1()->CreateShaderResourceView(this->texture_, &srvDesc, &this->shaderResourceView_))) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ShaderResourceView for back buffer Texture2D");
			
			this->texture_.reset();
			return false;
		}

		this->isShaderResourceViewSupported_ = true;
		this->flags_ = rend::Texture2DFlags::RenderTarget | rend::Texture2DFlags::ShaderResource;
	} else {
		this->isShaderResourceViewSupported_ = false;
		this->flags_ = rend::Texture2DFlags::RenderTarget;
	}

	return true;
}

bool rend::Texture2D::copyResource(rend::Texture2D* destination) const {
	if (!this->texture_ || !destination || !destination->texture_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Cannot copy texture resource: source or destination texture is null");
		return false;
	}

	rend::D3D11Device::getInstance().getDeviceContext1()->CopyResource(destination->texture_, this->texture_);
	return true;
}

bool rend::Texture2D::readData(uint8_t* destinationBuffer, uint32_t destinationBufferSize) const {
	if (!this->texture_ || !destinationBuffer)
		return false;

	size_t dataSize = this->getTotalSize();
	if (destinationBufferSize < dataSize) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unable to read data from texture \"%s\": buffer size is too small", this->getDebugName());
		return false;
	}

	uint8_t pixelSize = this->getBytesPerPixel();
	if (pixelSize == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Texture format has a unknown pixel size", this->getDebugName());
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ID3D11DeviceContext* deviceContext = rend::D3D11Device::getInstance().getDeviceContext1();
	HRESULT result = deviceContext->Map(this->texture_, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map texture \"%s\" for reading. HRESULT: 0x%08X", this->getDebugName(), result);
		return false;
	}

	const size_t rowBytes = static_cast<size_t>(this->width_) * pixelSize;
	for (uint32_t y = 0; y < this->height_; ++y) {
		uint8_t* destinationRow = destinationBuffer + static_cast<size_t>(y) * rowBytes;
		const uint8_t* sourceRow = static_cast<const uint8_t*>(mappedResource.pData) + static_cast<size_t>(y) * mappedResource.RowPitch;
		
		memcpy(destinationRow, sourceRow, rowBytes);
	}

	deviceContext->Unmap(this->texture_, 0);
	return true;
}

bool rend::Texture2D::writeData(const uint8_t* sourceBuffer, uint32_t sourceBufferSize) {
	if (!this->texture_ || !sourceBuffer)
		return false;

	size_t dataSize = this->getTotalSize();
	if (sourceBufferSize < dataSize) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unable to write data to texture \"%s\": buffer size is too small", this->getDebugName());
		return false;
	}

	uint8_t pixelSize = this->getBytesPerPixel();
	if (pixelSize == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unable to write data to texture \"%s\": invalid pixel size", this->getDebugName());
		return false;
	}

	rend::D3D11Device::getInstance().getDeviceContext1()->UpdateSubresource(this->texture_, 0, nullptr, sourceBuffer, this->width_ * pixelSize, 0);
	return true;
}

bool rend::Texture2D::readDataRegion(uint8_t* destinationBuffer, size_t destinationBufferSize, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t destinationPitch) const {
	if (!this->texture_ || !destinationBuffer)
		return false;

	if (x + width > this->width_ || y + height > this->height_)
		return false;

	size_t pixelSize = this->getBytesPerPixel();
	if (pixelSize == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Texture format has a unknown pixel size", this->getDebugName());
		return false;
	}

	size_t regionSize = static_cast<size_t>(height) * destinationPitch;
	if (destinationBufferSize < regionSize) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Cannot read data region from texture \"%s\" if the destination buffer is too small", this->getDebugName());
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	rend::RefCountPtr<ID3D11DeviceContext> deviceContext = rend::D3D11Device::getInstance().getDeviceContext1();
	HRESULT result = deviceContext->Map(this->texture_, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map texture \"%s\" for reading. HRESULT: 0x%08X", this->getDebugName(), result);
		return false;
	}

	for (UINT row = 0; row < height; ++row) {
		const BYTE* sourceRow = static_cast<const BYTE*>(mappedResource.pData) + static_cast<size_t>(y + row) * mappedResource.RowPitch + static_cast<size_t>(x) * pixelSize;
		BYTE* destinationRow = destinationBuffer + static_cast<size_t>(row) * destinationPitch;

		memcpy(destinationRow, sourceRow, static_cast<size_t>(width) * pixelSize);
	}

	deviceContext->Unmap(this->texture_, 0);
	return true;
}

bool rend::Texture2D::writeDataRegion(const uint8_t* sourceBuffer, size_t sourceBufferSize, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t sourcePitch) const {
	if (!this->texture_ || !sourceBuffer)
		return false;

	if (x + width > this->width_ || y + height > this->height_)
		return false;

	uint8_t pixelSize = this->getBytesPerPixel();
	if (pixelSize == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Texture format has a unknown pixel size", this->getDebugName());
		return false;
	}

	size_t regionSize = static_cast<size_t>(width) * height * pixelSize;
	if (sourceBufferSize < regionSize) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unable to write data region to texture \"%s\": buffer size is too small", this->getDebugName());
		return false;
	}
	D3D11_TEXTURE2D_DESC desc;
	this->texture_->GetDesc(&desc);

	rend::RefCountPtr<ID3D11DeviceContext> deviceContext = rend::D3D11Device::getInstance().getDeviceContext1();
	if (desc.Usage == D3D11_USAGE_DYNAMIC) {
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (FAILED(deviceContext->Map(this->texture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map texture \"%s\" for writing", this->getDebugName());
			return false;
		}

		const size_t rowBytes = static_cast<size_t>(width) * pixelSize;
		for (uint32_t row = 0; row < height; ++row) {
			uint8_t* destinationData = static_cast<uint8_t*>(mapped.pData) + static_cast<size_t>(y + row) * mapped.RowPitch + static_cast<size_t>(x) * pixelSize;
			const uint8_t* sourceData = sourceBuffer + static_cast<size_t>(row) * sourcePitch;
			
			memcpy(destinationData, sourceData, rowBytes);
		}

		deviceContext->Unmap(this->texture_, 0);
	} else {
		D3D11_BOX box;
		ZeroMemory(&box, sizeof(D3D11_BOX));
		box.left = x;
		box.top = y;
		box.front = 0;
		box.right = x + width;
		box.bottom = y + height;
		box.back = 1;

		deviceContext->UpdateSubresource(this->texture_, 0, &box, sourceBuffer, sourcePitch, 0);
	}

	return true;
}