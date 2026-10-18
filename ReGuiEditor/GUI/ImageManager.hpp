#pragma once
#include <FreeImage/FreeImage.h>

#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <UTILS/Singleton.hpp>

#include <ImGui/imgui.h>

#include <d2d1_1.h>
#include <d2d1_3.h>
#include <d2d1svg.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ImageManager : public UTILS::Singleton<ImageManager> {
	struct Image {
		FIBITMAP* bitmap = nullptr;
		rend::Texture2DHandle texture;
		uint32_t unusedFrames = 0;
		bool textureFailed = false;

		Image() = default;
		Image(Image&& other) noexcept : bitmap(std::exchange(other.bitmap, nullptr)), texture(std::move(other.texture)), unusedFrames(other.unusedFrames), textureFailed(other.textureFailed) {}
		
		~Image() {
			if (this->bitmap)
				FreeImage_Unload(this->bitmap);
		}

		bool isTextureInUse() const {
			return this->texture != nullptr && this->texture->GetRefCount() > 1;
		}

		void update() {
			this->unusedFrames = this->isTextureInUse() ? 0 : this->unusedFrames + 1;
		}

		bool isUnused() const {
			return this->unusedFrames > 40 && !this->isTextureInUse();
		}
	};

	std::unordered_map<std::string, Image> images_;

	rend::RefCountPtr<ID2D1Device> d2dDevice_ = nullptr;
	rend::RefCountPtr<ID2D1DeviceContext5> d2dContext_ = nullptr;

	Image* findOrLoadEntry(const std::string& path);

	bool renderSVG(const std::vector<char>& svg, uint32_t width, uint32_t height, ImU32 color, std::vector<uint8_t>& pixels);
	bool loadSVG(const std::string& key, const std::string& path, uint32_t width, uint32_t height, ImU32 color);

	void fitSVGToSize(ID2D1SvgDocument* svgDocument, uint32_t& width, uint32_t& height);
	void setSVGColor(ID2D1SvgDocument* svgDocument, ImU32 color);

	rend::Texture2DHandle createTexture(FIBITMAP* bitmap, const std::string& path);
	rend::Texture2DHandle createTextureFromBGRA(const void* pixels, uint32_t width, uint32_t height, const std::string& path);
public:
	ImageManager() = default;
	~ImageManager() { this->cleanup(); };

	bool initialize();
	void cleanup();

	void update();

	bool loadImage(const std::string& path);

	FIBITMAP* findOrLoadImage(const std::string& path);

	rend::Texture2DHandle findOrLoadTexture(const std::string& path);
	ImTextureID findOrLoadImGuiTexture(const std::string& path) {
		rend::Texture2DHandle texture = this->findOrLoadTexture(path);
		return texture ? reinterpret_cast<ImTextureID>(texture.get()) : ImTextureID();
	}

	rend::Texture2DHandle findOrLoadTextureSVG(const std::string& path, uint32_t width, uint32_t height, ImU32 color = IM_COL32_BLACK);
	ImTextureID findOrLoadImGuiTextureSVG(const std::string& path, uint32_t width, uint32_t height, ImU32 color = IM_COL32_BLACK) {
		rend::Texture2DHandle texture = this->findOrLoadTextureSVG(path, width, height, color);
		return texture ? reinterpret_cast<ImTextureID>(texture.get()) : ImTextureID();
	}
};