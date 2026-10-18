#include "Splashscreen.hpp"

#include <FreeImage/FreeImage.h>

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

constexpr const wchar_t* SPLASH_CLASS_NAME = L"REGUI_EDITOR_SPLASHSCREEN_CLASS";

constexpr int SPLASH_TEXT_PADDING = 16;
constexpr int SPLASH_TEXT_SIZE = 14;

constexpr double SPLASH_MAX_SCREEN_FRACTION = 0.8;

bool Splashscreen::loadBitmap(const std::filesystem::path& imagePath) {
	FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeU(imagePath.c_str());
	if (format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilenameU(imagePath.c_str());

	if (format == FIF_UNKNOWN) {
		CONSOLE_ERROR(L"Unknown splashscreen image format: %ls", imagePath.c_str());
		return false;
	}

	FIBITMAP* loadedImage = FreeImage_LoadU(format, imagePath.c_str());
	if (!loadedImage) {
		CONSOLE_ERROR(L"Failed to load splashscreen image: %ls", imagePath.c_str());
		return false;
	}

	FIBITMAP* image = FreeImage_ConvertTo32Bits(loadedImage);
	FreeImage_Unload(loadedImage);
	if (!image) {
		CONSOLE_ERROR(L"Failed to convert splashscreen image to 32 bits: %ls", imagePath.c_str());
		return false;
	}

	const double maxWidth = GetSystemMetrics(SM_CXSCREEN) * SPLASH_MAX_SCREEN_FRACTION;
	const double maxHeight = GetSystemMetrics(SM_CYSCREEN) * SPLASH_MAX_SCREEN_FRACTION;

	const double imageWidth = FreeImage_GetWidth(image);
	const double imageHeight = FreeImage_GetHeight(image);

	const double scale = std::min({ 1.0, maxWidth / imageWidth, maxHeight / imageHeight });
	if (scale < 1.0) {
		FIBITMAP* scaledImage = FreeImage_Rescale(image, std::max(1, static_cast<int>(imageWidth * scale)), std::max(1, static_cast<int>(imageHeight * scale)), FILTER_BICUBIC);
		FreeImage_Unload(image);

		image = scaledImage;
		if (!image) {
			CONSOLE_ERROR(L"Failed to scale splashscreen image: %ls", imagePath.c_str());
			return false;
		}
	}

	this->width_ = static_cast<int>(FreeImage_GetWidth(image));
	this->height_ = static_cast<int>(FreeImage_GetHeight(image));

	HDC screenDc = GetDC(nullptr);
	this->bitmap_ = CreateDIBitmap(screenDc, FreeImage_GetInfoHeader(image), CBM_INIT, FreeImage_GetBits(image), FreeImage_GetInfo(image), DIB_RGB_COLORS);
	ReleaseDC(nullptr, screenDc);

	FreeImage_Unload(image);

	if (!this->bitmap_) {
		CONSOLE_ERROR(L"Failed to create splashscreen bitmap");
		return false;
	}

	return true;
}

void Splashscreen::paint() {
	PAINTSTRUCT paintStruct;
	HDC windowDc = BeginPaint(this->hwnd_, &paintStruct);

	HDC bufferDc = CreateCompatibleDC(windowDc);
	HBITMAP bufferBitmap = CreateCompatibleBitmap(windowDc, this->width_, this->height_);
	HGDIOBJ previousBuffer = SelectObject(bufferDc, bufferBitmap);

	HDC imageDc = CreateCompatibleDC(windowDc);
	HGDIOBJ previousImage = SelectObject(imageDc, this->bitmap_);
	BitBlt(bufferDc, 0, 0, this->width_, this->height_, imageDc, 0, 0, SRCCOPY);
	SelectObject(imageDc, previousImage);
	DeleteDC(imageDc);

	if (!this->statusText_.empty()) {
		const int padding = MulDiv(SPLASH_TEXT_PADDING, GetDpiForWindow(this->hwnd_), 96);
		const RECT textRectangle = { padding, padding, this->width_ - padding, this->height_ - padding };

		RECT shadowRectangle = textRectangle;
		OffsetRect(&shadowRectangle, 1, 1);

		HGDIOBJ previousFont = SelectObject(bufferDc, this->font_);
		SetBkMode(bufferDc, TRANSPARENT);

		constexpr UINT textFormat = DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;

		SetTextColor(bufferDc, RGB(0, 0, 0));
		DrawTextW(bufferDc, this->statusText_.c_str(), -1, &shadowRectangle, textFormat);

		SetTextColor(bufferDc, RGB(255, 255, 255));
		DrawTextW(bufferDc, this->statusText_.c_str(), -1, const_cast<RECT*>(&textRectangle), textFormat);

		SelectObject(bufferDc, previousFont);
	}

	BitBlt(windowDc, 0, 0, this->width_, this->height_, bufferDc, 0, 0, SRCCOPY);

	SelectObject(bufferDc, previousBuffer);
	DeleteObject(bufferBitmap);
	DeleteDC(bufferDc);

	EndPaint(this->hwnd_, &paintStruct);
}

LRESULT CALLBACK Splashscreen::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_NCCREATE) {
		const CREATESTRUCT* createStruct = reinterpret_cast<const CREATESTRUCT*>(lParam);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
	}

	Splashscreen* splashscreen = reinterpret_cast<Splashscreen*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (splashscreen) {
		switch (message) {
		case WM_PAINT:
			splashscreen->paint();
			return 0;
		case WM_ERASEBKGND:
			return 1;
		}
	}

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool Splashscreen::show(const std::filesystem::path& imagePath) {
	if (this->hwnd_)
		return true;

	if (!this->loadBitmap(imagePath))
		return false;

	HINSTANCE instance = GetModuleHandle(nullptr);

	WNDCLASSW windowClass;
	ZeroMemory(&windowClass, sizeof(windowClass));
	windowClass.lpfnWndProc = Splashscreen::windowProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = SPLASH_CLASS_NAME;

	if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		CONSOLE_ERROR(L"Failed to register splashscreen window class");
		this->close();
		return false;
	}

	const int windowX = (GetSystemMetrics(SM_CXSCREEN) - this->width_) / 2;
	const int windowY = (GetSystemMetrics(SM_CYSCREEN) - this->height_) / 2;

	this->hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, SPLASH_CLASS_NAME, L"ReGui Layout Editor", WS_POPUP, windowX, windowY, this->width_, this->height_, nullptr, nullptr, instance, this);
	if (!this->hwnd_) {
		CONSOLE_ERROR(L"Failed to create splashscreen window");
		this->close();
		return false;
	}

	const int dpi = static_cast<int>(GetDpiForWindow(this->hwnd_));
	this->font_ = CreateFontW(-MulDiv(SPLASH_TEXT_SIZE, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

	ShowWindow(this->hwnd_, SW_SHOW);
	UpdateWindow(this->hwnd_);
	return true;
}

void Splashscreen::close() {
	if (this->hwnd_) {
		DestroyWindow(this->hwnd_);
		this->hwnd_ = nullptr;
	}

	if (this->font_) {
		DeleteObject(this->font_);
		this->font_ = nullptr;
	}

	if (this->bitmap_) {
		DeleteObject(this->bitmap_);
		this->bitmap_ = nullptr;
	}

	this->statusText_.clear();
}

void Splashscreen::setStatus(const std::wstring text) {
	if (!this->hwnd_)
		return;

	this->statusText_ = std::move(text);

	InvalidateRect(this->hwnd_, nullptr, FALSE);
	UpdateWindow(this->hwnd_);

	MSG message;
	while (PeekMessageW(&message, this->hwnd_, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}
