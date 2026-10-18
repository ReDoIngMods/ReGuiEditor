#pragma once
#include <UTILS/Singleton.hpp>

#include <rend/FullscreenPass.hpp>
#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/SamplerState.hpp>
#include <rend/Resources/ShaderResourceView.hpp>

#include <MyGui/MyGUI_Types.h>

class BlurrySubSkinRenderer : public UTILS::Singleton<BlurrySubSkinRenderer> {
	rend::FullscreenPass fullscreenPass_;
	rend::SamplerStateHandle samplerState_;
public:
	bool initialize();
	void cleanup();

	void composite(const rend::ShaderResourceView* backdropView, const MyGUI::FloatRect& backdropUV, const rend::ShaderResourceView* atlasView, const MyGUI::FloatRect& atlasUV, rend::RenderView* destinationView, int destinationWidth, int destinationHeight);
};
