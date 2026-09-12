#pragma once

#include <string>

namespace regui::canvas {

// Writes the editor's own MyGUI assets (editor-fonts.xml, editor-skins.xml,
// editor-white.png - compiled into the executable, see cmake/embed_binary.cmake)
// out to a directory under the system temp path, and returns that directory.
std::string extractEmbeddedEditorAssets();

} // namespace regui::canvas
