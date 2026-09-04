#pragma once
// MyGUI's OpenGL3Platform includes <GL/gl.h> with GL_GLEXT_PROTOTYPES defined,
// which only works when the platform's own GL headers export real symbols for
// modern/extension entry points (true on Linux/Mesa, not on Windows opengl32.dll).
// Redirecting to epoxy's drop-in replacement gives every entry point as a
// lazily-resolved function pointer, so linking against `epoxy` instead of
// `opengl32` resolves cleanly on MinGW. This header is prepended (BEFORE) onto
// MyGUI.OpenGL3Platform's include path in the top-level CMakeLists.txt.
#include <epoxy/gl.h>
