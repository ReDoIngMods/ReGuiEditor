#include "canvas/embedded_assets.h"

#include <embedded_editor_fonts_xml.h>
#include <embedded_editor_skins_xml.h>
#include <embedded_editor_white_png.h>

#include <filesystem>
#include <fstream>

namespace regui::canvas {

namespace {

void writeFile(const std::filesystem::path& path, const unsigned char* data, unsigned long len) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
}

} // namespace

std::string extractEmbeddedEditorAssets() {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "ReGuiEditor" / "editor-assets";
    std::filesystem::create_directories(dir);

    writeFile(dir / "editor-fonts.xml", kEmbeddedEditorFontsXml, kEmbeddedEditorFontsXml_len);
    writeFile(dir / "editor-skins.xml", kEmbeddedEditorSkinsXml, kEmbeddedEditorSkinsXml_len);
    writeFile(dir / "editor-white.png", kEmbeddedEditorWhitePng, kEmbeddedEditorWhitePng_len);

    return dir.string();
}

} // namespace regui::canvas
