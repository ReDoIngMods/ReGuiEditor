#pragma once
#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>

#include <rend/Common/RefCounter.hpp>

#include <rend/Resources/PipelineContext.hpp>
#include <rend/Resources/RenderView.hpp>

namespace MyGUICustom {
	class DirectX11Texture;

	class DirectX11RTTexture : public MyGUI::IRenderTarget {
		DirectX11Texture* texture_ = nullptr;
		rend::RefCountPtr<rend::PipelineContext> pipelineContext_;
		rend::RefCountPtr<rend::RenderView> renderView_;
		MyGUI::RenderTargetInfo renderTargetInfo_{};
		bool isActive_ = false;
	public:
		explicit DirectX11RTTexture(DirectX11Texture* texture);

		void begin() override;
		void end() override;

		void doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) override;

		const MyGUI::RenderTargetInfo& getInfo() override { return this->renderTargetInfo_; }
	};
}
