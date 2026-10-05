#pragma once
#include "MGRTTexture.hpp" // VCIC-Excluded: Visual studio's include cleaner suxxxx

#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderFormat.h>

#include <rend/Resources/Texture2D.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MyGUICustom {
	class DirectX11Texture : public MyGUI::ITexture {
		friend class MyGUICustom::DirectX11RTTexture;

		rend::Texture2DHandle texture_;
		std::unique_ptr<MyGUICustom::DirectX11RTTexture> renderTarget_;

		std::vector<uint8_t> pixels_;
		bool isLocked_ = false;

		int width_ = 0;
		int height_ = 0;

		int textureWidth_ = 0;
		int textureHeight_ = 0;

		MyGUI::TextureUsage usage_ = MyGUI::TextureUsage::Default;
		std::string name_;

		static std::string getCacheFilepath(const std::string& _filename);
		bool loadCache(const std::string& _filename);
		bool storeCache(const std::string& _filename, const uint8_t* _pixels);

		bool createBacking(int _width, int _height, rend::Texture2DFlags _flags);

		bool upload(const uint8_t* _pixels, size_t _size);
	public:
		explicit DirectX11Texture(const std::string& _name) : name_(_name) {}
		~DirectX11Texture() override { this->destroy(); }

		const std::string& getName() const override { return this->name_; }

		void createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) override;
		void loadFromFile(const std::string& _filename) override;
		void saveToFile(const std::string& _filename) override;
		void destroy() override;

		void* lock(MyGUI::TextureUsage _access) override;
		void unlock() override;
		bool isLocked() override { return this->isLocked_; }

		int getWidth() override { return this->width_; }
		int getHeight() override { return this->height_; }

		int getTextureWidth() const { return this->textureWidth_; }
		int getTextureHeight() const { return this->textureHeight_; }

		MyGUI::PixelFormat getFormat() override { return MyGUI::PixelFormat::R8G8B8A8; }
		MyGUI::TextureUsage getUsage() override { return this->usage_; }
		size_t getNumElemBytes() override { return 4; }

		MyGUI::IRenderTarget* getRenderTarget() override;

		rend::Texture2DHandle getTexture() const { return this->texture_; }
	};
}