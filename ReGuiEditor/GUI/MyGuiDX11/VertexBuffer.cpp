#include "RenderManager.hpp"
#include "VertexBuffer.hpp"
#include <MyGui/MyGUI_VertexData.h>
#include <Windows.h>
#include <d3d11.h>

MyGUI::Vertex* MyGUICustom::DirectX11VertexBuffer::lock() {
	if (this->buffer == nullptr) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::lock: Buffer is not created.");
		return nullptr;
	}

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11DeviceContext* context = renderManager.context;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = context->Map(this->buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::lock: Failed to map buffer.");
		return nullptr;
	}

	return static_cast<MyGUI::Vertex*>(mappedResource.pData);
}

void MyGUICustom::DirectX11VertexBuffer::unlock() {
	if (this->buffer == nullptr) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::unlock: Buffer is not created.");
		return;
	}

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11DeviceContext* context = renderManager.context;

	context->Unmap(this->buffer, 0);
}

bool MyGUICustom::DirectX11VertexBuffer::create() {
	if (this->buffer != nullptr) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::create: Buffer is already created.");
		return false;
	}

	if (this->needVertexCount == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::create: Vertex count is zero.");
		return false;
	}

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11Device* device = renderManager.device;

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = static_cast<UINT>(sizeof(MyGUI::Vertex) * this->needVertexCount);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT result = device->CreateBuffer(&bufferDesc, nullptr, &this->buffer);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11VertexBuffer::create: Failed to create vertex buffer. HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

void MyGUICustom::DirectX11VertexBuffer::destroy() {
	if (this->buffer) {
		this->buffer->Release();
		this->buffer = nullptr;
	}
}