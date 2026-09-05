cmake_minimum_required(VERSION 3.20)
project(ReGuiEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)

# FreeImage has no pkg-config file on MSYS2; locate it directly.
find_path(FREEIMAGE_INCLUDE_DIR NAMES FreeImage.h)
find_library(FREEIMAGE_LIBRARY NAMES freeimage)
if(NOT FREEIMAGE_INCLUDE_DIR OR NOT FREEIMAGE_LIBRARY)
    message(FATAL_ERROR "FreeImage not found (pacman -S mingw-w64-x86_64-freeimage)")
endif()

# --- MyGUI (vendored, built in-tree; not packaged for MSYS2) ---
set(MYGUI_RENDERSYSTEM 7 CACHE STRING "OpenGL 3.x" FORCE) # see third_party/mygui/CMakeLists.txt for the full list
set(MYGUI_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(MYGUI_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MYGUI_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
set(MYGUI_BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
set(MYGUI_BUILD_TEST_APP OFF CACHE BOOL "" FORCE)
set(MYGUI_BUILD_WRAPPER OFF CACHE BOOL "" FORCE)
set(MYGUI_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(MYGUI_USE_FREETYPE ON CACHE BOOL "" FORCE)
set(MYGUI_STATIC ON CACHE BOOL "" FORCE)

add_subdirectory(third_party/mygui)

# MinGW's opengl32 import lib only exports GL 1.1; redirect MyGUI's GL/gl.h
# includes to libepoxy so modern/extension entry points resolve at link time.
# See cmake/gl_shim/GL/gl.h for the rationale.
if(TARGET MyGUI.OpenGL3Platform)
    target_include_directories(MyGUI.OpenGL3Platform BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/cmake/gl_shim)
    target_link_libraries(MyGUI.OpenGL3Platform PkgConfig::EPOXY)
endif()

# --- ReLayout/ReAddon format library (no GTK/MyGUI dependency) ---
add_library(regui_format STATIC
    src/format/json_value.cpp
    src/format/parser.cpp
    src/format/writer.cpp
)
target_include_directories(regui_format PUBLIC src)

# --- Domain model: WidgetNode tree + ReAddonRegistry, built on regui_format ---
add_library(regui_model STATIC
    src/model/layout.cpp
    src/model/addon.cpp
)
target_include_directories(regui_model PUBLIC src)
target_link_libraries(regui_model PUBLIC regui_format)

add_executable(reguieditor
    src/main.cpp
    src/canvas/freeimage_loader.cpp
    src/canvas/mygui_canvas.cpp
    src/ui/accent_style.cpp
    src/ui/asset_setup.cpp
    src/ui/center_window.cpp
    src/ui/detachable_panel.cpp
    src/ui/game_assets.cpp
    src/ui/hierarchy_panel.cpp
    src/ui/history_window.cpp
    src/ui/layout_settings.cpp
    src/ui/loading_window.cpp
    src/ui/main_window.cpp
    src/ui/properties_panel.cpp
    src/ui/recent_files.cpp
    src/ui/skin_picker_dialog.cpp
    src/ui/welcome_window.cpp
    src/ui/widget_type_picker_dialog.cpp
)
target_include_directories(reguieditor PRIVATE
    third_party/mygui/MyGUIEngine/include
    third_party/mygui/Platforms/OpenGL3/OpenGL3Platform/include
    third_party/mygui/Common
    ${FREEIMAGE_INCLUDE_DIR}
)
target_link_libraries(reguieditor PRIVATE
    PkgConfig::GTK4
    PkgConfig::EPOXY
    MyGUI.OpenGL3Platform
    MyGUIEngine
    regui_model
    ${FREEIMAGE_LIBRARY}
)

# No console window on a normal launch (double-click or a shortcut) - main()
# reopens one itself, only when started with -dev, via AllocConsole().
if(WIN32)
    set_target_properties(reguieditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# MyGUI's OpenGL3 shaders live in the vendored source tree; point at them
# directly for now (dev-only). A real install/bundling step comes later once
# the app has its own resource layout to ship.
target_compile_definitions(reguieditor PRIVATE
    MYGUI_MEDIA_DIR="${CMAKE_SOURCE_DIR}/third_party/mygui/Media/MyGUI_Media"
    REGUIEDITOR_ASSETS_ROOT="${CMAKE_SOURCE_DIR}/assets"
    REGUIEDITOR_ADDONS_ROOT="${CMAKE_SOURCE_DIR}/addons"
)

# --- Bundle: copy the exe + all required runtime DLLs into their own
# subdirectory of the build tree, so it can be zipped/run standalone. ---
get_filename_component(REGUIEDITOR_TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(REGUIEDITOR_TOOLCHAIN_SHARE_DIR "${REGUIEDITOR_TOOLCHAIN_BIN_DIR}/../share" ABSOLUTE)
set(REGUIEDITOR_BUNDLE_DIR "${CMAKE_BINARY_DIR}/reguieditor")
add_custom_command(TARGET reguieditor POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DTARGET_FILE=$<TARGET_FILE:reguieditor>
        -DDEST_DIR=${REGUIEDITOR_BUNDLE_DIR}
        -DSEARCH_DIRS=${REGUIEDITOR_TOOLCHAIN_BIN_DIR}
        -DSHARE_DIR=${REGUIEDITOR_TOOLCHAIN_SHARE_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/bundle_runtime_deps.cmake"
    COMMENT "Bundling reguieditor.exe with its runtime DLLs into ${REGUIEDITOR_BUNDLE_DIR}"
    VERBATIM
)

enable_testing()
add_executable(format_tests tests/format_tests.cpp)
target_link_libraries(format_tests PRIVATE regui_format)
add_test(NAME format_tests COMMAND format_tests "${CMAKE_SOURCE_DIR}/examples")

add_executable(model_tests tests/model_tests.cpp)
target_link_libraries(model_tests PRIVATE regui_model)
add_test(NAME model_tests COMMAND model_tests "${CMAKE_SOURCE_DIR}/examples" "${CMAKE_CURRENT_BINARY_DIR}")
