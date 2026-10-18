#pragma once
#include <Windows.h>

#include <filesystem>
#include <string>

class Splashscreen {
	HWND hwnd_ = nullptr;
	HBITMAP bitmap_ = nullptr;
	HFONT font_ = nullptr;

	int width_ = 0;
	int height_ = 0;

	std::wstring statusText_;

	bool loadBitmap(const std::filesystem::path& imagePath);
	void paint();

	static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
public:
	Splashscreen() = default;
	~Splashscreen() { this->close(); }

	Splashscreen(const Splashscreen&) = delete;
	Splashscreen& operator=(const Splashscreen&) = delete;

	bool show(const std::filesystem::path& imagePath);
	void close();

	void setStatus(const std::wstring text);

	bool isShown() const { return this->hwnd_ != nullptr; }
};
