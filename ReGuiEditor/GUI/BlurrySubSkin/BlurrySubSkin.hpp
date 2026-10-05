#pragma once
#include "GuiBlurSkinTexture.hpp"

#include <GUI/MyGuiDX11/MGTexture.hpp>

#include <MyGui/MyGUI_ILayerNode.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RTTI.h>
#include <MyGui/MyGUI_SubSkin.h>

#include <rend/Resources/ShaderResourceView.hpp>

class BlurrySubSkin : public MyGUI::SubSkin {
	MYGUI_RTTI_DERIVED(BlurrySubSkin)

	GuiBlurSkinTexture compositeTexture_;
	MyGUICustom::DirectX11Texture* atlasTexture_ = nullptr;

	int compositeWidth_ = 1;
	int compositeHeight_ = 1;

	bool ensureCompositeTarget(int width, int height);
public:
	BlurrySubSkin() = default;
	~BlurrySubSkin() override = default;

	void createDrawItem(MyGUI::ITexture* _texture, MyGUI::ILayerNode* _node) override;
	void destroyDrawItem() override;

	void doRender() override;

	void updateComposite(const rend::ShaderResourceView* backdropView, int canvasWidth, int canvasHeight);
};
