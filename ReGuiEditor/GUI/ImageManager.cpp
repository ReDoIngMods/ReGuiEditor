#include "ImageManager.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <rend/Common/Format.hpp>
#include <rend/Common/RefCounter.hpp>
#include <rend/D3D11Device.hpp>
#include <rend/ResourceManager.hpp>
#include <rend/Resources/Resource.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <FreeImage/FreeImage.h>

#include <ImGui/imgui.h>

#include <Windows.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1helper.h>
#include <d2d1svg.h>
#include <d3d11_1.h>
#include <dcommon.h>
#include <dxgi.h>
#include <dxgiformat.h>
#include <shlwapi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "d2d1.lib")

constexpr uint32_t EVICT_IMAGE_AFTER_CERTAIN_AMOUNT = 40;

inline static rend::Texture2DHandle createTexture(FIBITMAP* bitmap, const std::string& path) {
	FIBITMAP* bitmap32 = FreeImage_ConvertTo32Bits(bitmap);
	if (!bitmap32) {
		CONSOLE_ERROR("Failed to convert '%s' to 32 bits.", path.c_str());
		return nullptr;
	}

	FreeImage_FlipVertical(bitmap32);

	const uint32_t width = FreeImage_GetWidth(bitmap32);
	const uint32_t height = FreeImage_GetHeight(bitmap32);

	rend::Texture2DHandle texture = rend::ResourceManager::getInstance().createTexture2D(width, height, rend::Format::B8G8R8A8_UNORM, rend::Texture2DFlags::ShaderResource);
	const bool uploaded = texture && texture->writeData(FreeImage_GetBits(bitmap32), static_cast<size_t>(width) * height * 4);
	FreeImage_Unload(bitmap32);

	if (!uploaded) {
		CONSOLE_ERROR("Failed to create texture for '%s'.", path.c_str());
		return nullptr;
	}

	texture->setDebugName(path.c_str());
	return texture;
}

inline static rend::Texture2DHandle createTextureFromBGRA(const void* pixels, uint32_t width, uint32_t height, const std::string& path) {
	rend::Texture2DHandle texture = rend::ResourceManager::getInstance().createTexture2D(width, height, rend::Format::B8G8R8A8_UNORM, rend::Texture2DFlags::ShaderResource);
	if (!texture) {
		CONSOLE_ERROR("Failed to create texture for '%s'.", path.c_str());
		return nullptr;
	}

	if (!texture->writeData(reinterpret_cast<const uint8_t*>(pixels), static_cast<size_t>(width) * height * 4)) {
		CONSOLE_ERROR("Failed to write data to texture for '%s'.", path.c_str());
		return nullptr;
	}

	texture->setDebugName(path.c_str());
	return texture;
}

rend::Texture2DHandle Image::getTexture() {
	if (!this->texture_ && this->bitmap_ && !this->textureFailed_) {
		this->texture_ = createTexture(this->bitmap_.get(), this->path_);
		this->textureFailed_ = !this->texture_;
	}

	return this->texture_;
}

bool ImageManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing ImageManager...");

	rend::D3D11Device& d3d11Device = rend::D3D11Device::getInstance();
	ID3D11Device1* device1 = d3d11Device.getDevice1();

	rend::RefCountPtr<IDXGIDevice> dxgiDevice = nullptr;
	HRESULT result = device1->QueryInterface(IID_PPV_ARGS(dxgiDevice.getAddressOf()));
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to get IDXGIDevice from ID3D11Device1. HRESULT: 0x%08X", result);
		return false;
	}

	result = D2D1CreateDevice(dxgiDevice, nullptr, this->d2dDevice_.getAddressOf());
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create ID2D1Device. HRESULT: 0x%08X", result);
		return false;
	}

	rend::RefCountPtr<ID2D1DeviceContext> d2dContext = nullptr;
	result = this->d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dContext.getAddressOf());
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create ID2D1DeviceContext. HRESULT: 0x%08X", result);
		return false;
	}

	result = d2dContext->QueryInterface(IID_PPV_ARGS(this->d2dContext_.getAddressOf()));
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to get ID2D1DeviceContext5 from ID2D1DeviceContext. HRESULT: 0x%08X", result);
		return false;
	}

	return true;
}

void ImageManager::cleanup() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Cleaning up ImageManager...");
	this->images_.clear();
}

void ImageManager::update() {
	for (auto iterator = this->images_.begin(); iterator != this->images_.end();) {
		CacheEntry& entry = iterator->second;
		entry.unusedFrames = entry.image.use_count() > 1 ? 0 : entry.unusedFrames + 1;

		if (entry.unusedFrames > EVICT_IMAGE_AFTER_CERTAIN_AMOUNT)
			iterator = this->images_.erase(iterator);
		else
			++iterator;
	}
}

ImagePtr ImageManager::findOrLoadImage(const std::string& path) {
	auto cached = this->images_.find(path);
	if (cached != this->images_.end()) {
		cached->second.unusedFrames = 0;
		return cached->second.image;
	}

	std::vector<uint8_t> fileContent;
	if (!FilesystemManager::getInstance().readFile(path, fileContent)) {
		CONSOLE_ERROR("Failed to read image file '%s'.", path.c_str());
		return nullptr;
	}

	FIMEMORY* memory = FreeImage_OpenMemory(fileContent.data(), static_cast<DWORD>(fileContent.size()));
	if (!memory) {
		CONSOLE_ERROR("Failed to open image memory for '%s'.", path.c_str());
		return nullptr;
	}

	FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(memory, 0);
	if (format == FIF_UNKNOWN) {
		CONSOLE_ERROR("Unknown image format for '%s'.", path.c_str());

		FreeImage_CloseMemory(memory);
		return nullptr;
	}

	FIBITMAP* bitmap = FreeImage_LoadFromMemory(format, memory, 0);
	FreeImage_CloseMemory(memory);
	if (!bitmap) {
		CONSOLE_ERROR("Failed to load image from memory for '%s'.", path.c_str());
		return nullptr;
	}

	ImagePtr image = std::make_shared<Image>(path, bitmap);
	this->images_.emplace(path, CacheEntry{ image, 0 });
	return image;
}

bool ImageManager::renderSVG(const std::vector<char>& svg, uint32_t& width, uint32_t& height, ImU32 color, std::vector<uint8_t>& pixels) {
	rend::RefCountPtr<IStream> stream = nullptr;
	stream.attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), static_cast<UINT>(svg.size())));
	if (!stream) {
		CONSOLE_ERROR("Failed to create IStream from SVG data.");
		return false;
	}

	const D2D1_PIXEL_FORMAT format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);

	const bool hasWidth = width != IMAGE_MANAGER_SVG_NO_SIZE_CHANGE;
	const bool hasHeight = height != IMAGE_MANAGER_SVG_NO_SIZE_CHANGE;

	rend::RefCountPtr<ID2D1SvgDocument> svgDocument = nullptr;
	HRESULT result = this->d2dContext_->CreateSvgDocument(stream, D2D1::SizeF(hasWidth ? static_cast<FLOAT>(width) : 1.f, hasHeight ? static_cast<FLOAT>(height) : 1.f), &svgDocument);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create ID2D1SvgDocument. HRESULT: 0x%08X", result);
		return false;
	}

	if (!hasWidth || !hasHeight) {
		float naturalWidth = 0.f;
		float naturalHeight = 0.f;
		if (!this->getSVGNaturalSize(svgDocument, naturalWidth, naturalHeight)) {
			CONSOLE_ERROR("Failed to determine natural SVG size.");
			return false;
		}

		if (hasWidth)
			height = std::max(1u, static_cast<uint32_t>(std::lround(width * naturalHeight / naturalWidth)));
		else if (hasHeight)
			width = std::max(1u, static_cast<uint32_t>(std::lround(height * naturalWidth / naturalHeight)));
		else {
			width = std::max(1u, static_cast<uint32_t>(std::lround(naturalWidth)));
			height = std::max(1u, static_cast<uint32_t>(std::lround(naturalHeight)));
		}

		svgDocument->SetViewportSize(D2D1::SizeF(static_cast<FLOAT>(width), static_cast<FLOAT>(height)));
	}

	if (width == 0 || height == 0) {
		CONSOLE_ERROR("Invalid SVG render size %ux%u.", width, height);
		return false;
	}

	const D2D1_SIZE_U size = D2D1::SizeU(width, height);

	this->fitSVGToSize(svgDocument, width, height);
	this->setSVGColor(svgDocument, color);

	rend::RefCountPtr<ID2D1Bitmap1> bitmap = nullptr;
	result = this->d2dContext_->CreateBitmap(size, nullptr, 0, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, format), &bitmap);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create ID2D1Bitmap1. HRESULT: 0x%08X", result);
		return false;
	}

	rend::RefCountPtr<ID2D1Bitmap1> stagingBitmap = nullptr;
	result = this->d2dContext_->CreateBitmap(size, nullptr, 0, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ, format), &stagingBitmap);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create staging ID2D1Bitmap1. HRESULT: 0x%08X", result);
		return false;
	}

	this->d2dContext_->SetTarget(bitmap);
	this->d2dContext_->BeginDraw();
	this->d2dContext_->Clear(D2D1::ColorF(0, 0.f));
	this->d2dContext_->DrawSvgDocument(svgDocument);
	result = this->d2dContext_->EndDraw();
	this->d2dContext_->SetTarget(nullptr);

	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to draw SVG document. HRESULT: 0x%08X", result);
		return false;
	}

	result = stagingBitmap->CopyFromBitmap(nullptr, bitmap, nullptr);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to copy bitmap to staging bitmap. HRESULT: 0x%08X", result);
		return false;
	}

	D2D1_MAPPED_RECT mappedRectangle;
	result = stagingBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedRectangle);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to map staging bitmap. HRESULT: 0x%08X", result);
		return false;
	}

	pixels.resize(static_cast<size_t>(width) * height * 4);

	for (uint32_t y = 0; y < height; ++y) {
		const uint8_t* sourceRow = reinterpret_cast<const uint8_t*>(mappedRectangle.bits) + y * mappedRectangle.pitch;
		uint8_t* destinationRow = pixels.data() + y * width * 4;

		for (uint32_t x = 0; x < width; ++x) {
			const uint8_t* sourcePixel = sourceRow + x * 4;

			const uint8_t alpha = sourcePixel[3];
			if (alpha != 0 && alpha != 255) {
				destinationRow[x * 4 + 0] = static_cast<uint8_t>(std::min(255, sourcePixel[0] * 255 / alpha));
				destinationRow[x * 4 + 1] = static_cast<uint8_t>(std::min(255, sourcePixel[1] * 255 / alpha));
				destinationRow[x * 4 + 2] = static_cast<uint8_t>(std::min(255, sourcePixel[2] * 255 / alpha));
			}
			else {
				destinationRow[x * 4 + 0] = sourcePixel[0];
				destinationRow[x * 4 + 1] = sourcePixel[1];
				destinationRow[x * 4 + 2] = sourcePixel[2];
			}

			destinationRow[x * 4 + 3] = alpha;
		}
	}

	stagingBitmap->Unmap();
	return true;
}

ImagePtr ImageManager::findOrLoadImageSVG(const std::string& path, uint32_t width, uint32_t height, ImU32 color) {
	const std::string key = path + '@' + std::to_string(width) + 'x' + std::to_string(height) + '#' + std::to_string(color);

	auto cached = this->images_.find(key);
	if (cached != this->images_.end()) {
		cached->second.unusedFrames = 0;
		return cached->second.image;
	}

	std::vector<char> fileContent;
	if (!FilesystemManager::getInstance().readFile(path, fileContent)) {
		CONSOLE_ERROR("Failed to read SVG file '%s'.", path.c_str());
		return nullptr;
	}

	std::vector<uint8_t> pixels;
	if (!this->renderSVG(fileContent, width, height, color, pixels)) {
		CONSOLE_ERROR("Failed to render SVG '%s'.", path.c_str());
		return nullptr;
	}

	rend::Texture2DHandle texture = createTextureFromBGRA(pixels.data(), width, height, key);
	if (!texture)
		return nullptr;

	ImagePtr image = std::make_shared<Image>(key, std::move(texture), width, height);
	this->images_.emplace(key, CacheEntry{ image, 0 });
	return image;
}

bool ImageManager::getSVGNaturalSize(ID2D1SvgDocument* svgDocument, float& width, float& height) {
	rend::RefCountPtr<ID2D1SvgElement> root = nullptr;
	svgDocument->GetRoot(&root);

	if (!root)
		return false;

	bool hasWidth = false;
	bool hasHeight = false;

	D2D1_SVG_LENGTH svgWidth;
	HRESULT result = root->GetAttributeValue(L"width", D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH, &svgWidth, sizeof(svgWidth));
	if (SUCCEEDED(result) && svgWidth.units != D2D1_SVG_LENGTH_UNITS_PERCENTAGE && svgWidth.value > 0.f) {
		width = svgWidth.value;
		hasWidth = true;
	}

	D2D1_SVG_LENGTH svgHeight;
	result = root->GetAttributeValue(L"height", D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH, &svgHeight, sizeof(svgHeight));
	if (SUCCEEDED(result) && svgHeight.units != D2D1_SVG_LENGTH_UNITS_PERCENTAGE && svgHeight.value > 0.f) {
		height = svgHeight.value;
		hasHeight = true;
	}

	if (hasWidth && hasHeight)
		return true;

	D2D1_SVG_VIEWBOX viewBox;
	result = root->GetAttributeValue(L"viewBox", D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX, &viewBox, sizeof(viewBox));
	if (SUCCEEDED(result) && viewBox.width > 0.f && viewBox.height > 0.f) {
		width = viewBox.width;
		height = viewBox.height;
		return true;
	}

	return false;
}

void ImageManager::fitSVGToSize(ID2D1SvgDocument* svgDocument, uint32_t& width, uint32_t& height) {
	rend::RefCountPtr<ID2D1SvgElement> root = nullptr;
	svgDocument->GetRoot(&root);

	if (!root)
		return;

	if (!root->IsAttributeSpecified(L"viewBox")) {
		D2D1_SVG_LENGTH svgWidth;
		HRESULT result = root->GetAttributeValue(L"width", D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH, &svgWidth, sizeof(svgWidth));
		if (FAILED(result)) {
			CONSOLE_ERROR("Failed to get SVG width attribute. HRESULT: 0x%08X", result);
			return;
		}

		D2D1_SVG_LENGTH svgHeight;
		result = root->GetAttributeValue(L"height", D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH, &svgHeight, sizeof(svgHeight));
		if (FAILED(result)) {
			CONSOLE_ERROR("Failed to get SVG height attribute. HRESULT: 0x%08X", result);
			return;
		}

		if (svgWidth.units == D2D1_SVG_LENGTH_UNITS_PERCENTAGE || svgHeight.units == D2D1_SVG_LENGTH_UNITS_PERCENTAGE || svgWidth.value <= 0.f || svgHeight.value <= 0.f)
			return;

		const D2D1_SVG_VIEWBOX viewBox = { 0.f, 0.f, svgWidth.value, svgHeight.value };
		result = root->SetAttributeValue(L"viewBox", D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX, &viewBox, sizeof(viewBox));
		if (FAILED(result)) {
			CONSOLE_ERROR("Failed to set SVG viewBox attribute. HRESULT: 0x%08X", result);
			return;
		}
	}

	const D2D1_SVG_LENGTH targetWidth = { static_cast<FLOAT>(width), D2D1_SVG_LENGTH_UNITS_NUMBER };
	HRESULT result = root->SetAttributeValue(L"width", D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH, &targetWidth, sizeof(targetWidth));
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to set SVG width attribute. HRESULT: 0x%08X", result);
		return;
	}

	const D2D1_SVG_LENGTH targetHeight = { static_cast<FLOAT>(height), D2D1_SVG_LENGTH_UNITS_NUMBER };
	result = root->SetAttributeValue(L"height", D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH, &targetHeight, sizeof(targetHeight));
	if (FAILED(result))
		CONSOLE_ERROR("Failed to set SVG height attribute. HRESULT: 0x%08X", result);
}

void ImageManager::setSVGColor(ID2D1SvgDocument* svgDocument, ImU32 color) {
	rend::RefCountPtr<ID2D1SvgElement> root = nullptr;
	svgDocument->GetRoot(&root);

	if (!root)
		return;

	D2D1_COLOR_F value = {
		.r = static_cast<FLOAT>((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.f,
		.g = static_cast<FLOAT>((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.f,
		.b = static_cast<FLOAT>((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.f,
		.a = static_cast<FLOAT>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.f
	};

	const HRESULT result = root->SetAttributeValue(L"color", D2D1_SVG_ATTRIBUTE_POD_TYPE_COLOR, &value, sizeof(value));
	if (FAILED(result))
		CONSOLE_ERROR("Failed to set SVG color attribute. HRESULT: 0x%08X", result);
}