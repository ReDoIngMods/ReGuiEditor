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
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define IMAGE_MANAGER_SVG_NO_SIZE_CHANGE static_cast<uint32_t>(-1)

class Image {
	std::string path_;
	std::unique_ptr<FIBITMAP, decltype(&FreeImage_Unload)> bitmap_{ nullptr, &FreeImage_Unload };
	rend::Texture2DHandle texture_;

	uint32_t width_ = 0;
	uint32_t height_ = 0;
	bool textureFailed_ = false;
public:
	Image(std::string path, FIBITMAP* bitmap) : path_(std::move(path)), bitmap_(bitmap, &FreeImage_Unload), width_(FreeImage_GetWidth(bitmap)), height_(FreeImage_GetHeight(bitmap)) {}
	Image(std::string path, rend::Texture2DHandle texture, uint32_t width, uint32_t height) : path_(std::move(path)), texture_(std::move(texture)), width_(width), height_(height) {}

	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;

	FIBITMAP* getBitmap() const {
		return this->bitmap_.get();
	}

	rend::Texture2DHandle getTexture();

	ImTextureID getTextureId() {
		rend::Texture2DHandle texture = this->getTexture();
		return texture ? reinterpret_cast<ImTextureID>(texture.get()) : ImTextureID();
	}

	float getWidth() const { return static_cast<float>(this->width_); }
	float getHeight() const { return static_cast<float>(this->height_); }
	float getAspectRatio() const { return this->width_ > 0 ? static_cast<float>(this->width_) / static_cast<float>(this->height_) : 1.f; }
};

using ImagePtr = std::shared_ptr<Image>;

class ImageManager : public UTILS::Singleton<ImageManager> {
	struct CacheEntry {
		ImagePtr image;
		uint32_t unusedFrames = 0;
	};

	std::unordered_map<std::string, CacheEntry> images_;

	rend::RefCountPtr<ID2D1Device> d2dDevice_ = nullptr;
	rend::RefCountPtr<ID2D1DeviceContext5> d2dContext_ = nullptr;

	bool renderSVG(const std::vector<char>& svg, uint32_t& width, uint32_t& height, ImU32 color, std::vector<uint8_t>& pixels);

	bool getSVGNaturalSize(ID2D1SvgDocument* svgDocument, float& width, float& height);

	void fitSVGToSize(ID2D1SvgDocument* svgDocument, uint32_t& width, uint32_t& height);
	void setSVGColor(ID2D1SvgDocument* svgDocument, ImU32 color);
public:
	ImageManager() = default;
	~ImageManager() { this->cleanup(); };

	bool initialize();
	void cleanup();

	void update();

	ImagePtr findOrLoadImage(const std::string& path);
	ImagePtr findOrLoadImageSVG(const std::string& path, uint32_t width = IMAGE_MANAGER_SVG_NO_SIZE_CHANGE, uint32_t height = IMAGE_MANAGER_SVG_NO_SIZE_CHANGE, ImU32 color = IM_COL32_BLACK);
};