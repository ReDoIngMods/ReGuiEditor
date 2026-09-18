#pragma once
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_VertexData.h>

#include <d3d11.h>

namespace MyGUICustom {
	class DirectX11VertexBuffer : public MyGUI::IVertexBuffer {
		size_t vertexCount = 0;
		size_t needVertexCount = 0;
	public:
		ID3D11Buffer* buffer = nullptr;

		~DirectX11VertexBuffer() { this->destroy(); }

		virtual void setVertexCount(size_t _count) {
			if (this->vertexCount != _count) {
				this->needVertexCount = _count;

				this->destroy();
				this->create();
			}
		}

		virtual size_t getVertexCount() { return this->vertexCount; }

		virtual MyGUI::Vertex* lock();
		virtual void unlock();

		bool create();
		void destroy();
	};
}