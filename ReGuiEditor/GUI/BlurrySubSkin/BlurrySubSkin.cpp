#include "BlurrySubSkin.hpp"

#include "../Application.hpp"

#include "BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkinRenderer.hpp"

#include <GUI/MyGuiDX11/Texture.hpp>

#include <MyGui/MyGUI_Diagnostic.h>
#include <MyGui/MyGUI_ILayerNode.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderItem.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>
#include <MyGui/MyGUI_SubSkin.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_VertexData.h>

#include <UTILS/Strings.hpp>

#include <Windows.h>

#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

#include <string>

BlurrySubSkin::BlurrySubSkin() : MyGUI::SubSkin() {
}

BlurrySubSkin::~BlurrySubSkin() {
	if (this->compositeTargetView_) {
		this->compositeTargetView_->Release();
		this->compositeTargetView_ = nullptr;
	}
}

bool BlurrySubSkin::ensureCompositeTarget(int width, int height) {
	if (width == this->compositeWidth_ && height == this->compositeHeight_ && this->compositeTargetView_) {
		return true;
	}

	if (this->compositeTargetView_) {
		this->compositeTargetView_->Release();
		this->compositeTargetView_ = nullptr;
	}

	Application& app = Application::getInstance();
	ID3D11Device* device = app.d3d11Device.getDevice();

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D* texture = nullptr;
	HRESULT result = device->CreateTexture2D(&desc, nullptr, &texture);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BlurrySubSkin::ensureCompositeTarget: Failed to create texture. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = desc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	result = device->CreateRenderTargetView(texture, &rtvDesc, &this->compositeTargetView_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BlurrySubSkin::ensureCompositeTarget: Failed to create RTV. HRESULT: 0x%X", result);
		
		texture->Release();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	ID3D11ShaderResourceView* shaderResourceView = nullptr;
	result = device->CreateShaderResourceView(texture, &srvDesc, &shaderResourceView);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BlurrySubSkin::ensureCompositeTarget: Failed to create SRV. HRESULT: 0x%X", result);

		this->compositeTargetView_->Release();
		this->compositeTargetView_ = nullptr;

		texture->Release();
		return false;
	}

	this->compositeTexture_.update(texture, shaderResourceView);
	texture->Release();
	shaderResourceView->Release();

	this->compositeWidth_ = width;
	this->compositeHeight_ = height;

	return true;
}

void BlurrySubSkin::createDrawItem(MyGUI::ITexture* _texture, MyGUI::ILayerNode* _node) {
	MYGUI_ASSERT(!mRenderItem, "mRenderItem must be nullptr");

	this->atlasTexture_ = static_cast<MyGUICustom::DirectX11Texture*>(_texture);

	std::string textureName = UTILS::formatString("BlurrySubSkin_%p", this);
	this->compositeTexture_.initialize(textureName, this->compositeWidth_, this->compositeHeight_);

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

	if (this->compositeTargetView_) {
		this->compositeTargetView_->Release();
		this->compositeTargetView_ = nullptr;
	}

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

void BlurrySubSkin::updateComposite(ID3D11ShaderResourceView* backdropView, int canvasWidth, int canvasHeight) {
	if (!this->mVisible || this->mEmptyView || !this->mRenderItem)
		return;

	if (!this->atlasTexture_ || !this->atlasTexture_->shaderResourceView)
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

	BlurrySubSkinRenderer::getInstance().composite(backdropView, backdropUV, this->atlasTexture_->shaderResourceView, this->mCurrentTexture, this->compositeTargetView_, width, height);
}
