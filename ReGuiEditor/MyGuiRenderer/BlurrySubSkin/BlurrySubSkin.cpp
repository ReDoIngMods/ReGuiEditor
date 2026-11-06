#include "BlurrySubSkin.hpp"

#include "BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkinRenderer.hpp"

#include <rend/Resources/ShaderResourceView.hpp>

#include "../RenderPlatform/RPTexture.hpp"

#include <UTILS/Strings.hpp>

#include <MyGui/MyGUI_Diagnostic.h>
#include <MyGui/MyGUI_ILayerNode.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderItem.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>
#include <MyGui/MyGUI_SubSkin.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_VertexData.h>

#include <string>

bool BlurrySubSkin::ensureCompositeTarget(int width, int height) {
	if (width == this->compositeWidth_ && height == this->compositeHeight_)
		return true;

	if (!this->compositeTexture_.resize(width, height))
		return false;

	this->compositeWidth_ = width;
	this->compositeHeight_ = height;
	return true;
}

void BlurrySubSkin::createDrawItem(MyGUI::ITexture* _texture, MyGUI::ILayerNode* _node) {
	MYGUI_ASSERT(!mRenderItem, "mRenderItem must be nullptr");

	this->atlasTexture_ = static_cast<RPTexture*>(_texture);

	std::string textureName = UTILS::formatString("BlurrySubSkin_%p", this);
	const bool isCompositeCreated = this->compositeTexture_.initialize(textureName, this->compositeWidth_, this->compositeHeight_);
	MYGUI_ASSERT(isCompositeCreated, "Failed to create blur composite texture");

	MyGUI::ITexture* ourTexture = MyGUI::RenderManager::getInstance().getTexture(this->compositeTexture_.getName());
	this->mNode = _node;
	this->mRenderItem = this->mNode->addToRenderItem(ourTexture, true, this->mSeparate);
	this->mRenderItem->addDrawItem(this, MyGUI::VertexQuad::VertexCount);

	BlurrySubSkinRegistry::getInstance().addInstance(this);
}

void BlurrySubSkin::destroyDrawItem() {
	MYGUI_ASSERT(this->mRenderItem, "mRenderItem must be not nullptr");

	BlurrySubSkinRegistry::getInstance().removeInstance(this);

	this->mNode = nullptr;
	this->mRenderItem->removeDrawItem(this);
	this->mRenderItem = nullptr;

	this->compositeTexture_.cleanup();

	this->compositeWidth_ = 1;
	this->compositeHeight_ = 1;
	this->atlasTexture_ = nullptr;
}

void BlurrySubSkin::doRender() {
	if (!this->mVisible || this->mEmptyView)
		return;

	MyGUI::VertexQuad* vertexQuad = reinterpret_cast<MyGUI::VertexQuad*>(this->mRenderItem->getCurrentVertexBuffer());
	const MyGUI::RenderTargetInfo& info = this->mRenderItem->getRenderTarget()->getInfo();

	float parentLeft = static_cast<float>(this->mCroppedParent->getAbsoluteLeft());
	float parentTop = static_cast<float>(this->mCroppedParent->getAbsoluteTop());

	float left = static_cast<float>(this->mCurrentCoord.left + parentLeft - info.leftOffset);
	float top = static_cast<float>(this->mCurrentCoord.top + parentTop - info.topOffset);

	float scaledLeft = info.pixScaleX * left;
	float scaledTop = info.pixScaleY * top;

	float vertexLeft = ((scaledLeft + info.hOffset) * 2) - 1;
	float vertexRight = vertexLeft + (info.pixScaleX * static_cast<float>(this->mCurrentCoord.width) * 2);

	float vertexTop = -(((scaledTop + info.vOffset) * 2) - 1);
	float vertexBottom = vertexTop - (info.pixScaleY * static_cast<float>(this->mCurrentCoord.height) * 2);

	vertexQuad->set(vertexLeft, vertexTop, vertexRight, vertexBottom, info.maximumDepth, 0.f, 0.f, 1.f, 1.f, mCurrentColour);
	this->mRenderItem->setLastVertexCount(MyGUI::VertexQuad::VertexCount);
}

void BlurrySubSkin::updateComposite(const rend::ShaderResourceView* backdropView, int canvasWidth, int canvasHeight) {
	if (!this->mVisible || this->mEmptyView || !this->mRenderItem)
		return;

	if (!this->atlasTexture_ || !this->atlasTexture_->getTexture())
		return;

	int width = this->mCurrentCoord.width;
	int height = this->mCurrentCoord.height;
	if (width <= 0 || height <= 0)
		return;

	if (!this->ensureCompositeTarget(width, height))
		return;

	int absoluteLeft = this->mCurrentCoord.left + this->mCroppedParent->getAbsoluteLeft();
	int absoluteTop = this->mCurrentCoord.top + this->mCroppedParent->getAbsoluteTop();

	MyGUI::FloatRect backdropUV;
	backdropUV.left = absoluteLeft / static_cast<float>(canvasWidth);
	backdropUV.top = absoluteTop / static_cast<float>(canvasHeight);
	backdropUV.right = (absoluteLeft + width) / static_cast<float>(canvasWidth);
	backdropUV.bottom = (absoluteTop + height) / static_cast<float>(canvasHeight);

	const float atlasScaleX = static_cast<float>(this->atlasTexture_->getWidth()) / static_cast<float>(this->atlasTexture_->getTextureWidth());
	const float atlasScaleY = static_cast<float>(this->atlasTexture_->getHeight()) / static_cast<float>(this->atlasTexture_->getTextureHeight());

	MyGUI::FloatRect atlasUV = this->mCurrentTexture;
	atlasUV.left *= atlasScaleX;
	atlasUV.right *= atlasScaleX;
	atlasUV.top *= atlasScaleY;
	atlasUV.bottom *= atlasScaleY;

	BlurrySubSkinRenderer::getInstance().composite(backdropView, backdropUV, this->atlasTexture_->getTexture().get(), atlasUV, this->compositeTexture_.getRenderView(), width, height);
}