#pragma once
#include "RTTexture.hpp" // VCIC-Excluded: Visual studio's include cleaner suxxxx

#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderFormat.h>

#include <d3d11.h>

#include <string>


namespace MyGUICustom {
	class DirectX11Texture : public MyGUI::ITexture {
		friend class MyGUICustom::DirectX11RTTexture;
	public:
		ID3D11Texture2D* texture = nullptr;
		ID3D11ShaderResourceView* shaderResourceView = nullptr;
	private:
		MyGUICustom::DirectX11RTTexture* renderTarget_ = nullptr;

		D3D11_MAPPED_SUBRESOURCE mappedResource_{};
		bool isLocked_ = false;

		int width_ = 0;
		int height_ = 0;

		int realWidth_ = 0;
		int realHeight_ = 0;

		MyGUI::TextureUsage textureUsage_ = MyGUI::TextureUsage::Default;

		std::string name_;

		void* writeData_ = nullptr;

		std::string getCacheFilepath(const std::string& _filename);

		bool loadCache(const std::string& _filename);
		bool storeCache(const std::string& _filename);
	public:
		DirectX11Texture(const std::string& _name) : name_(_name) {}
		virtual ~DirectX11Texture() { this->destroy(); }

		virtual const std::string& getName() const { return this->name_; }

		virtual void createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format);
		virtual void loadFromFile(const std::string& _filename);
		virtual void saveToFile(const std::string& _filename) {}

		virtual void destroy();

		virtual void* lock(MyGUI::TextureUsage _access);
		virtual void unlock();
		virtual bool isLocked() { return this->isLocked_; }

		virtual int getWidth() { return this->width_; }
		virtual int getHeight() { return this->height_; }

		int getRealWidth() const { return this->realWidth_; }
		int getRealHeight() const { return this->realHeight_; }

		virtual MyGUI::PixelFormat  getFormat() { return MyGUI::PixelFormat::R8G8B8A8; }
		virtual MyGUI::TextureUsage getUsage() { return this->textureUsage_; }
		virtual size_t getNumElemBytes() { return 4; }

		virtual MyGUI::IRenderTarget* getRenderTarget() {
			if (!this->renderTarget_)
				this->renderTarget_ = new MyGUICustom::DirectX11RTTexture(this);

			return this->renderTarget_;
		}
	};
}