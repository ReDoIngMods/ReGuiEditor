#include "MGVertexBuffer.hpp"

#include <rend/ResourceManager.hpp>

#include <rend/Resources/Buffers/BufferResource.hpp>

#include <UTILS/Console.hpp>

#include <MyGui/MyGUI_VertexData.h>

#include <atomic>
#include <cstdint>
#include <string>

MyGUICustom::DirectX11VertexBuffer::~DirectX11VertexBuffer() {
	this->unlock();
}

void MyGUICustom::DirectX11VertexBuffer::setVertexCount(size_t _count) {
	if (_count == this->vertexCount_ && this->buffer_)
		return;

	this->unlock();
	this->buffer_.reset();
	this->vertexCount_ = 0;

	if (_count == 0)
		return;

	this->buffer_ = rend::ResourceManager::getInstance().createVertexBuffer(sizeof(MyGUI::Vertex), static_cast<uint32_t>(_count));
	if (!this->buffer_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::setVertexCount: Failed to create vertex buffer for %zu vertices.", _count);
		return;
	}

	static std::atomic<uint32_t> nextBufferId = 0;
	if (this->id_ == 0)
		this->id_ = ++nextBufferId;

	this->buffer_->setDebugName("MyGUI::VertexBuffer #" + std::to_string(this->id_));

	this->vertexCount_ = _count;
}

MyGUI::Vertex* MyGUICustom::DirectX11VertexBuffer::lock() {
	if (!this->buffer_)
		return nullptr;

	if (this->mappedData_)
		return this->mappedData_;

	if (!this->buffer_->mapBuffer(&this->mappedData_, rend::BufferMapType::WriteDiscard)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::lock: Failed to map vertex buffer.");
		this->mappedData_ = nullptr;
	}

	return this->mappedData_;
}

void MyGUICustom::DirectX11VertexBuffer::unlock() {
	if (!this->mappedData_)
		return;

	if (!this->buffer_->unmapBuffer())
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::unlock: Failed to unmap vertex buffer.");

	this->mappedData_ = nullptr;
}