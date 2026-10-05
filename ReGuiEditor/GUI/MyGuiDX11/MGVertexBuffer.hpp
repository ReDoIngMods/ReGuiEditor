#pragma once
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_VertexData.h>

#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/Buffers/VertexBuffer.hpp>

#include <cstdint>

namespace MyGUICustom {
	class DirectX11VertexBuffer : public MyGUI::IVertexBuffer {
		rend::RefCountPtr<rend::VertexBuffer> buffer_;
		size_t vertexCount_ = 0;
		uint32_t id_ = 0; // Only used for debug names
		MyGUI::Vertex* mappedData_ = nullptr;
	public:
		~DirectX11VertexBuffer() override;

		void setVertexCount(size_t _count) override;
		size_t getVertexCount() override { return this->vertexCount_; }

		MyGUI::Vertex* lock() override;
		void unlock() override;

		const rend::RefCountPtr<rend::VertexBuffer>& getBuffer() const { return this->buffer_; }
	};
}
