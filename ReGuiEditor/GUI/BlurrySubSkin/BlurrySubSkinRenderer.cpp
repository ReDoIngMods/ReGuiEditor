#include "BlurrySubSkinRenderer.hpp"

#include <MyGui/MyGUI_Types.h>

#include <rend/Common/SamplerAddressMode.hpp>
#include <rend/Common/Viewport.hpp>
#include <rend/ResourceManager.hpp>

#include <UTILS/Console.hpp>
#include <rend/FullscreenPass.hpp>
#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/ShaderResourceView.hpp>

namespace {
	// Anonymous namespace: BackgroundImageHelper.cpp has a differently laid out struct with the same name, and two
	// definitions of one name with external linkage is an ODR violation.
	struct alignas(16) ConstantBufferData {
		float backdropUV[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
		float atlasUV[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	};
}

bool BlurrySubSkinRenderer::initialize() {
	if (this->fullscreenPass_.isValid())
		return true;

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	rend::FullscreenPassDesc passDesc;
	passDesc.pixelShader = "$RGE_DATA/Shaders/gui_blur_background.hlsl";
	passDesc.constantBufferSize = sizeof(ConstantBufferData);
	passDesc.debugName = "BlurrySubSkin";

	this->samplerState_ = resourceManager.createLinearSamplerState(rend::SamplerAddressMode::Clamp);

	if (!this->fullscreenPass_.init(passDesc) || !this->samplerState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BlurrySubSkinRenderer::initialize: Failed to create render resources.");

		this->cleanup();
		return false;
	}

	this->samplerState_->setDebugName("BlurrySubSkin::SamplerState");

	return true;
}

void BlurrySubSkinRenderer::cleanup() {
	this->samplerState_.reset();
	this->fullscreenPass_.cleanup();
}

void BlurrySubSkinRenderer::composite(const rend::ShaderResourceView* backdropView, const MyGUI::FloatRect& backdropUV, const rend::ShaderResourceView* atlasView, const MyGUI::FloatRect& atlasUV, rend::RenderView* destinationView, int destinationWidth, int destinationHeight) {
	if (!this->fullscreenPass_.isValid() || !destinationView)
		return;

	ConstantBufferData data;
	data.backdropUV[0] = backdropUV.left;
	data.backdropUV[1] = backdropUV.top;
	data.backdropUV[2] = backdropUV.right;
	data.backdropUV[3] = backdropUV.bottom;
	data.atlasUV[0] = atlasUV.left;
	data.atlasUV[1] = atlasUV.top;
	data.atlasUV[2] = atlasUV.right;
	data.atlasUV[3] = atlasUV.bottom;

	rend::FullscreenDrawParams params;
	params.target = destinationView;
	params.viewport = rend::Viewport(0.0f, 0.0f, static_cast<float>(destinationWidth), static_cast<float>(destinationHeight), 0.0f, 1.0f);
	params.constants = &data;
	params.shaderResourceViews = { backdropView, atlasView };
	params.samplerStates = { this->samplerState_.get() };

	this->fullscreenPass_.draw(params);
}
