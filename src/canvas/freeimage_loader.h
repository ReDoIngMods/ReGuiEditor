#pragma once

#include <MyGUI_OpenGL3ImageLoader.h>

namespace regui::canvas {

// Decodes textures for MyGUI's OpenGL3Platform via FreeImage. Without one,
// loadFromFile() silently no-ops and every skin renders solid black.
class FreeImageLoader : public MyGUI::OpenGL3ImageLoader {
public:
    void* loadImage(int& width, int& height, MyGUI::PixelFormat& format, const std::string& filename) override;
    void saveImage(int width, int height, MyGUI::PixelFormat format, void* texture, const std::string& filename)
        override;
};

} // namespace regui::canvas
