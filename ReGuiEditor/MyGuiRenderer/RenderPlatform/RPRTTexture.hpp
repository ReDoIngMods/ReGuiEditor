#pragma once
#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>

#include <rend/Common/RefCounter.hpp>

#include <rend/Resources/PipelineContext.hpp>
#include <rend/Resources/RenderView.hpp>

class RPTexture;

class RPRTTexture : public MyGUI::IRenderTarget {
	RPTexture* texture_ = nullptr;
	rend::RefCountPtr<rend::PipelineContext> pipelineContext_;
	rend::RefCountPtr<rend::RenderView> renderView_;
	MyGUI::RenderTargetInfo renderTargetInfo_{};
	bool isActive_ = false;
public:
	explicit RPRTTexture(RPTexture* texture);

	void begin() override;
	void end() override;

	void doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) override;

	const MyGUI::RenderTargetInfo& getInfo() override { return this->renderTargetInfo_; }
};