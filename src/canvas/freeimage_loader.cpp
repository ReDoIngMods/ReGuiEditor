#include "canvas/freeimage_loader.h"

#include <FreeImage.h>

#include <MyGUI_OpenGL3DataManager.h>

#include <cstring>

namespace regui::canvas {

void* FreeImageLoader::loadImage(int& width, int& height, MyGUI::PixelFormat& format, const std::string& filename) {
    std::string path = MyGUI::OpenGL3DataManager::getInstance().getDataPath(filename);
    if (path.empty()) {
        return nullptr;
    }

    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(path.c_str(), 0);
    if (fif == FIF_UNKNOWN) {
        fif = FreeImage_GetFIFFromFilename(path.c_str());
    }
    if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif)) {
        return nullptr;
    }

    FIBITMAP* bitmap = FreeImage_Load(fif, path.c_str());
    if (!bitmap) {
        return nullptr;
    }

    FIBITMAP* rgba = FreeImage_ConvertTo32Bits(bitmap);
    FreeImage_Unload(bitmap);
    if (!rgba) {
        return nullptr;
    }

    width = static_cast<int>(FreeImage_GetWidth(rgba));
    height = static_cast<int>(FreeImage_GetHeight(rgba));
    format = MyGUI::PixelFormat::R8G8B8A8;

    // FreeImage stores DIBs bottom-up; flip while copying to row-0-on-top.
    // FI_RGBA_* indices account for FreeImage's platform channel order.
    // MyGUI's OpenGL(3)Texture maps PixelFormat::R8G8B8A8 to GL_BGRA (see
    // MyGUI_OpenGL3Texture.cpp), so despite the enum's name it expects bytes
    // in B,G,R,A order - not R,G,B,A - or every texture renders red/blue swapped.
    auto* out = new unsigned char[static_cast<size_t>(width) * height * 4];
    unsigned pitch = FreeImage_GetPitch(rgba);
    const unsigned char* base = FreeImage_GetBits(rgba);
    for (int y = 0; y < height; ++y) {
        const unsigned char* srcRow = base + static_cast<size_t>(height - 1 - y) * pitch;
        unsigned char* dstRow = out + static_cast<size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + FI_RGBA_BLUE];
            dstRow[x * 4 + 1] = srcRow[x * 4 + FI_RGBA_GREEN];
            dstRow[x * 4 + 2] = srcRow[x * 4 + FI_RGBA_RED];
            dstRow[x * 4 + 3] = srcRow[x * 4 + FI_RGBA_ALPHA];
        }
    }

    FreeImage_Unload(rgba);
    return out;
}

void FreeImageLoader::saveImage(int, int, MyGUI::PixelFormat, void*, const std::string&) {
    // Not needed: the editor never exports a render target/texture to disk.
}

} // namespace regui::canvas
