#pragma once
#include "GuiBlurSkinTexture.hpp"

#include <GUI/MyGuiDX11/Texture.hpp>

#include <MyGui/MyGUI_ILayerNode.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RTTI.h>
#include <MyGui/MyGUI_SubSkin.h>

#include <d3d11.h>

class BlurrySubSkin : public MyGUI::SubSkin {
	MYGUI_RTTI_DERIVED(BlurrySubSkin)

	GuiBlurSkinTexture compositeTexture_;
	ID3D11RenderTargetView* compositeTargetView_ = nullptr;
	MyGUICustom::DirectX11Texture* atlasTexture_ = nullptr;

	int compositeWidth_ = 1;
	int compositeHeight_ = 1;

	bool ensureCompositeTarget(int width, int height);
public:
	BlurrySubSkin();
	virtual ~BlurrySubSkin();

	virtual void createDrawItem(MyGUI::ITexture* _texture, MyGUI::ILayerNode* _node) override;
	virtual void destroyDrawItem() override;

	virtual void doRender() override;

	void updateComposite(ID3D11ShaderResourceView* backdropView, int canvasWidth, int canvasHeight);
};
