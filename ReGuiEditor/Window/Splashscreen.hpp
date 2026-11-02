#pragma once
#include <Windows.h>

#include <filesystem>
#include <string>

namespace ReGuiEditor {
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

		Splashscreen(Splashscreen&) = delete;
		Splashscreen& operator=(Splashscreen&) = delete;

		bool show(const std::filesystem::path& imagePath);
		void close();

		void setStatus(const std::wstring text, ...);
		void setStatus(const std::string text, ...);

		bool isShown() const { return this->hwnd_ != nullptr; }
	};

	extern Splashscreen gSplashscreen;
}

#define SET_SPLASH_STATUS(text, ...) ReGuiEditor::gSplashscreen.setStatus(text, ##__VA_ARGS__)