#pragma once
#include <FreeImage/FreeImage.h>

#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <UTILS/Singleton.hpp>

#include <ImGui/imgui.h>

#include <d2d1_1.h>
#include <d2d1_3.h>
#include <d2d1svg.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#define IMAGE_MANAGER_SVG_NO_SIZE_CHANGE static_cast<uint32_t>(-1)

class Image {
public:
	enum class State : uint8_t { Loading, Ready, Failed };

private:
	friend class ImageManager;

	std::string path_;
	std::unique_ptr<FIBITMAP, decltype(&FreeImage_Unload)> bitmap_{ nullptr, &FreeImage_Unload };
	std::vector<uint8_t> pixels_; // Decoded BGRA (SVG), released after upload.
	rend::Texture2DHandle texture_;

	uint32_t width_ = 0;
	uint32_t height_ = 0;
	std::atomic<State> state_{ State::Loading };
	bool textureFailed_ = false;

	// Worker thread only. Data is written before the release store that publishes it.
	void finishBitmap(FIBITMAP* bitmap);
	void finishPixels(std::vector<uint8_t> pixels, uint32_t width, uint32_t height);
	void fail() { this->state_.store(State::Failed, std::memory_order_release); }
public:
	explicit Image(std::string path) : path_(std::move(path)) {}

	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;

	State getState() const { return this->state_.load(std::memory_order_acquire); }
	bool isReady() const { return this->getState() == State::Ready; }

	FIBITMAP* getBitmap() const { return this->isReady() ? this->bitmap_.get() : nullptr; }

	rend::Texture2DHandle getTexture(); // Main/render thread only.

	ImTextureID getTextureId() {
		rend::Texture2DHandle texture = this->getTexture();
		return texture ? reinterpret_cast<ImTextureID>(texture.get()) : ImTextureID();
	}

	float getWidth() const { return this->isReady() ? static_cast<float>(this->width_) : 0.f; }
	float getHeight() const { return this->isReady() ? static_cast<float>(this->height_) : 0.f; }
	float getAspectRatio() const { return this->isReady() && this->height_ > 0 ? static_cast<float>(this->width_) / static_cast<float>(this->height_) : 1.f; }
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

	std::mutex mutex_;
	std::condition_variable_any condition_;
	std::queue<std::function<void()>> tasks_;
	std::jthread worker_;

	void enqueue(std::function<void()> task);
	void workerLoop(std::stop_token stop);

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