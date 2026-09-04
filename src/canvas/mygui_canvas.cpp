#include "canvas/mygui_canvas.h"

#include <epoxy/gl.h>

#include "canvas/freeimage_loader.h"

#include <MyGUI_Colour.h>
#include <MyGUI_Gui.h>
#include <MyGUI_OpenGL3Platform.h>
#include <MyGUI_ResourceManager.h>
#include <MyGUI_SkinManager.h>
#include <MyGUI_TextBox.h>
#include <MyGUI_Widget.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

namespace regui::canvas {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open '" + path + "'");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string basename(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string parentDir(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(0, slash);
}

// GuiConfig.xml's `<List file="$GAME_DATA/Gui/X.xml"/>` entries, re-resolved
// to just their basename against our own registered resource locations.
std::vector<std::string> parseListFiles(const std::string& xmlText) {
    std::vector<std::string> result;
    size_t pos = 0;
    while (true) {
        size_t tagPos = xmlText.find("<List", pos);
        if (tagPos == std::string::npos) {
            break;
        }
        size_t tagEnd = xmlText.find('>', tagPos);
        if (tagEnd == std::string::npos) {
            break;
        }
        std::string tag = xmlText.substr(tagPos, tagEnd - tagPos);
        size_t filePos = tag.find("file=\"");
        if (filePos != std::string::npos) {
            size_t valueStart = filePos + 6;
            size_t valueEnd = tag.find('"', valueStart);
            if (valueEnd != std::string::npos) {
                result.push_back(tag.substr(valueStart, valueEnd - valueStart));
            }
        }
        pos = tagEnd + 1;
    }
    return result;
}

// Strips the "$GAME_DATA/Gui/" prefix but keeps any subdirectory under it, needed to read the file off disk directly.
std::string relativePath(const std::string& entry) {
    constexpr const char* kPrefix = "$GAME_DATA/Gui/";
    return entry.rfind(kPrefix, 0) == 0 ? entry.substr(std::strlen(kPrefix)) : basename(entry);
}

// Extracts attr="value" from a single already-isolated opening-tag string
// (e.g. "<Resource type=\"ResourceSkin\" name=\"Foo\""), or "" if absent.
std::string attrValue(const std::string& tag, const std::string& attr) {
    std::string needle = attr + "=\"";
    size_t pos = tag.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    size_t valueStart = pos + needle.size();
    size_t valueEnd = tag.find('"', valueStart);
    return valueEnd == std::string::npos ? std::string() : tag.substr(valueStart, valueEnd - valueStart);
}

// Parses a whitespace-separated "x y w h" attribute into a Rect (0 if missing/malformed).
model::Rect parseRectAttr(const std::string& text) {
    std::istringstream in(text);
    model::Rect rect;
    in >> rect.x >> rect.y >> rect.width >> rect.height;
    return rect;
}

// Parses a whitespace-separated "w h" `size` attribute.
void parseSizeAttr(const std::string& text, double& width, double& height) {
    std::istringstream in(text);
    in >> width >> height;
}

// Decomposes a BasisSkin's `align` (e.g. "HStretch Top") into per-axis
// SkinAlign; unrecognized/missing tokens default to Begin (Left/Top).
void parseAlign(const std::string& text, SkinAlign& hAlign, SkinAlign& vAlign) {
    std::istringstream in(text);
    std::string token;
    while (in >> token) {
        if (token == "Left") {
            hAlign = SkinAlign::Begin;
        } else if (token == "Right") {
            hAlign = SkinAlign::End;
        } else if (token == "HStretch") {
            hAlign = SkinAlign::Stretch;
        } else if (token == "Top") {
            vAlign = SkinAlign::Begin;
        } else if (token == "Bottom") {
            vAlign = SkinAlign::End;
        } else if (token == "VStretch") {
            vAlign = SkinAlign::Stretch;
        } else if (token == "Stretch") {
            hAlign = SkinAlign::Stretch;
            vAlign = SkinAlign::Stretch;
        }
    }
}

// Finds BasisSkin pieces that are plain texture crops: ones whose chosen
// State ("normal" if present, else the first) declares an "offset" -
// procedural BasisSkin types never have one, so this needs no type allowlist.
std::vector<SkinPiece> collectSkinPieces(const std::string& blockText) {
    std::vector<SkinPiece> pieces;
    size_t pos = 0;
    while (true) {
        size_t tagPos = blockText.find("<BasisSkin", pos);
        if (tagPos == std::string::npos) {
            break;
        }
        size_t tagEnd = blockText.find('>', tagPos);
        if (tagEnd == std::string::npos) {
            break;
        }
        std::string openTag = blockText.substr(tagPos, tagEnd - tagPos);
        bool selfClosing = tagEnd > 0 && blockText[tagEnd - 1] == '/';
        std::string inner;
        size_t nextPos = tagEnd + 1;
        if (!selfClosing) {
            size_t closePos = blockText.find("</BasisSkin>", tagEnd);
            if (closePos == std::string::npos) {
                break;
            }
            inner = blockText.substr(tagEnd + 1, closePos - (tagEnd + 1));
            nextPos = closePos + std::strlen("</BasisSkin>");
        }
        pos = nextPos;

        std::string normalOffset, firstOffset;
        size_t statePos = 0;
        while (true) {
            size_t stateTagPos = inner.find("<State", statePos);
            if (stateTagPos == std::string::npos) {
                break;
            }
            size_t stateTagEnd = inner.find('>', stateTagPos);
            if (stateTagEnd == std::string::npos) {
                break;
            }
            std::string stateTag = inner.substr(stateTagPos, stateTagEnd - stateTagPos);
            statePos = stateTagEnd + 1;
            std::string offset = attrValue(stateTag, "offset");
            if (offset.empty()) {
                continue;
            }
            if (firstOffset.empty()) {
                firstOffset = offset;
            }
            if (attrValue(stateTag, "name") == "normal") {
                normalOffset = offset;
                break;
            }
        }
        std::string chosen = !normalOffset.empty() ? normalOffset : firstOffset;
        if (chosen.empty()) {
            continue; // procedural piece - no texture-space rect to sample
        }
        SkinPiece piece;
        piece.place = parseRectAttr(attrValue(openTag, "offset"));
        piece.textureRect = parseRectAttr(chosen);
        parseAlign(attrValue(openTag, "align"), piece.hAlign, piece.vAlign);
        pieces.push_back(piece);
    }
    return pieces;
}

// Scans a skin XML for every `<Resource type="ResourceSkin">` via substring
// search (MyGUI::SkinManager has no "list every skin" API). `textureDir` is
// where this XML's `texture="..."` filenames resolve from (siblings of the XML itself).
void collectSkinInfos(
    const std::string& xmlText, const std::string& category, const std::string& textureDir,
    std::vector<SkinInfo>& out) {
    size_t pos = 0;
    while (true) {
        size_t tagPos = xmlText.find("<Resource", pos);
        if (tagPos == std::string::npos) {
            break;
        }
        size_t tagEnd = xmlText.find('>', tagPos);
        if (tagEnd == std::string::npos) {
            break;
        }
        std::string openTag = xmlText.substr(tagPos, tagEnd - tagPos);
        size_t closePos = xmlText.find("</Resource>", tagEnd);
        if (closePos == std::string::npos) {
            break; // self-closing or malformed - no ResourceSkin in this project's assets does this
        }
        std::string blockText = xmlText.substr(tagEnd + 1, closePos - (tagEnd + 1));
        pos = closePos + std::strlen("</Resource>");

        if (attrValue(openTag, "type") != "ResourceSkin") {
            continue;
        }
        std::string name = attrValue(openTag, "name");
        if (name.empty()) {
            continue;
        }

        SkinInfo info;
        info.name = name;
        info.category = category;
        parseSizeAttr(attrValue(openTag, "size"), info.baseWidth, info.baseHeight);
        info.pieces = collectSkinPieces(blockText);
        std::string texture = attrValue(openTag, "texture");
        if (!texture.empty() && !info.pieces.empty()) {
            info.texturePath = textureDir + "/" + texture;
        }
        out.push_back(std::move(info));
    }
}

MyGUI::IntCoord toIntCoord(const model::Rect& rect) {
    return MyGUI::IntCoord(
        static_cast<int>(std::lround(rect.x)),
        static_cast<int>(std::lround(rect.y)),
        std::max(1, static_cast<int>(std::lround(rect.width))),
        std::max(1, static_cast<int>(std::lround(rect.height))));
}

constexpr const char* kFallbackSkin = "EditorFlatColour"; // see editor-skins.xml

// Falls back to a flat placeholder skin so an unrecognized widget stays visible.
std::string resolveSkin(const std::string& requested) {
    if (!requested.empty() && MyGUI::SkinManager::getInstance().isExist(requested)) {
        return requested;
    }
    return kFallbackSkin;
}

} // namespace

constexpr double kHandleSize = 8.0;
constexpr double kSnapThreshold = 10.0; // reference units - distance to *engage* a snap
constexpr double kUnsnapThreshold = 24.0; // reference units - distance to *release* an already-engaged Move snap

MyGuiCanvas::MyGuiCanvas(std::string assetsRoot) : mAssetsRoot(std::move(assetsRoot)) {
    mGlArea = gtk_gl_area_new();
    gtk_gl_area_set_required_version(GTK_GL_AREA(mGlArea), 3, 3);
    gtk_widget_set_hexpand(mGlArea, TRUE);
    gtk_widget_set_vexpand(mGlArea, TRUE);
    gtk_widget_set_focusable(mGlArea, TRUE);

    g_signal_connect(mGlArea, "realize", G_CALLBACK(realizeThunk), this);
    g_signal_connect(mGlArea, "unrealize", G_CALLBACK(unrealizeThunk), this);
    g_signal_connect(mGlArea, "resize", G_CALLBACK(resizeThunk), this);
    g_signal_connect(mGlArea, "render", G_CALLBACK(renderThunk), this);

    GtkGesture* dragGesture = gtk_gesture_drag_new();
    g_signal_connect(dragGesture, "drag-begin", G_CALLBACK(dragBeginThunk), this);
    g_signal_connect(dragGesture, "drag-update", G_CALLBACK(dragUpdateThunk), this);
    g_signal_connect(dragGesture, "drag-end", G_CALLBACK(dragEndThunk), this);
    gtk_widget_add_controller(mGlArea, GTK_EVENT_CONTROLLER(dragGesture));

    GtkGesture* rightClickGesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rightClickGesture), GDK_BUTTON_SECONDARY);
    g_signal_connect(rightClickGesture, "pressed", G_CALLBACK(rightClickThunk), this);
    gtk_widget_add_controller(mGlArea, GTK_EVENT_CONTROLLER(rightClickGesture));
}

void MyGuiCanvas::setRoots(std::vector<model::WidgetNode>* roots) {
    mRoots = roots;
    mSelected = nullptr; // the previous selection belongs to a different document now
    if (mGuiInitialised) {
        gtk_gl_area_make_current(GTK_GL_AREA(mGlArea));
        rebuildWidgets();
        gtk_widget_queue_draw(mGlArea);
    }
}

void MyGuiCanvas::setSelection(model::WidgetNode* node) {
    mSelected = node;
    if (mGuiInitialised) {
        gtk_gl_area_make_current(GTK_GL_AREA(mGlArea));
        rebuildOverlay();
        gtk_widget_queue_draw(mGlArea);
    }
}

void MyGuiCanvas::refresh() {
    if (mGuiInitialised) {
        gtk_gl_area_make_current(GTK_GL_AREA(mGlArea));
        rebuildWidgets();
        gtk_widget_queue_draw(mGlArea);
    }
}

void MyGuiCanvas::setReferenceResolution(double width, double height) {
    mReferenceWidth = width;
    mReferenceHeight = height;
    refresh();
}

void MyGuiCanvas::setGridSpacing(double spacing) {
    mGridSpacing = std::max(1.0, spacing); // a <=0 spacing would spin renderBackdropAndGrid()'s dot loop forever
    gtk_widget_queue_draw(mGlArea);
}

void MyGuiCanvas::setGridEnabled(bool enabled) {
    mGridEnabled = enabled;
    gtk_widget_queue_draw(mGlArea);
}

void MyGuiCanvas::onRealize() {
    gtk_gl_area_make_current(GTK_GL_AREA(mGlArea));
    if (GError* error = gtk_gl_area_get_error(GTK_GL_AREA(mGlArea))) {
        std::cerr << "GL context error: " << error->message << "\n";
        return;
    }

    reportLoadProgress("Initializing OpenGL platform", "", "MyGuiCanvas", 0.0);

    mPlatform = new MyGUI::OpenGL3Platform();

    // Registered before MyGUI's vendored Media dir: both share filenames
    // (MyGUI_Layers.xml), and DataManager resolves collisions to whichever registered first.
    MyGUI::OpenGL3DataManager* dataManager = mPlatform->getDataManagerPtr();
    dataManager->addResourceLocation(mAssetsRoot, false);
    dataManager->addResourceLocation(mAssetsRoot + "/MyGUI_Media", false);
    dataManager->addResourceLocation(mAssetsRoot + "/Fonts", false);
    dataManager->addResourceLocation(mAssetsRoot + "/Language/English", false);
    dataManager->addResourceLocation(parentDir(mAssetsRoot), false); // editor-fonts.xml
    dataManager->addResourceLocation(MYGUI_MEDIA_DIR, false); // MyGUI's own shaders only

    mImageLoader = new FreeImageLoader();
    // An empty log name skips OpenGL3Platform's own createDefaultSource()
    // call, so MyGUI never writes a MyGUI.log file into the working
    // directory at all - it otherwise appears next to whatever main.cpp is
    // simply launched from, in this app's case usually the repo root.
    mPlatform->initialise(mImageLoader, "");

    try {
        // Reserves 5%-90% of overall progress for this variable-length step; loadGameResources() only knows its own 0..1.
        loadGameResources([this](const std::string& what, const std::string& where, double localFraction) {
            reportLoadProgress(what, where, "MyGUI ResourceManager", 0.05 + localFraction * 0.85);
        });
    } catch (const std::exception& e) {
        std::cerr << "Failed to load game GUI resources: " << e.what() << "\n";
    }

    reportLoadProgress("Setting up grid renderer", "", "MyGuiCanvas", 0.92);
    setupGridRenderer();

    reportLoadProgress("Building widget tree", "", "MyGuiCanvas", 0.97);
    rebuildWidgets();

    reportLoadProgress("Done", "", "MyGuiCanvas", 1.0);
}

void MyGuiCanvas::reportLoadProgress(const std::string& what, const std::string& where, const std::string& who, double fraction) {
    if (mOnLoadProgress) {
        mOnLoadProgress({what, where, who, fraction});
    }
}

void MyGuiCanvas::onUnrealize() {
    gtk_gl_area_make_current(GTK_GL_AREA(mGlArea));
    teardownGridRenderer();
    destroyOverlay();
    destroyWidgets();
    if (mGuiInitialised) {
        MyGUI::Gui::getInstance().shutdown();
        delete MyGUI::Gui::getInstancePtr();
        mGuiInitialised = false;
    }
    if (mPlatform) {
        mPlatform->shutdown();
        delete mPlatform;
        mPlatform = nullptr;
    }
    delete mImageLoader;
    mImageLoader = nullptr;
}

void MyGuiCanvas::onResize(int width, int height) {
    mViewWidth = width;
    mViewHeight = height;
    if (mPlatform) {
        mPlatform->getRenderManagerPtr()->setViewSize(width, height);
    }
    glViewport(0, 0, width, height);
    rebuildWidgets(); // widget rects depend on canvas size (scale-based layout)
}

void MyGuiCanvas::onRender() {
    glClearColor(0.15f, 0.16f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderBackdropAndGrid(); // drawn first so MyGUI's widgets render on top of it
    if (mPlatform) {
        mPlatform->getRenderManagerPtr()->drawOneFrame();
    }
}

namespace {

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Grid shader compile error: " << log << "\n";
    }
    return shader;
}

} // namespace

void MyGuiCanvas::setupGridRenderer() {
    static const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)";
    static const char* kFragmentSrc = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";

    unsigned int vs = compileShader(GL_VERTEX_SHADER, kVertexSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFragmentSrc);
    mGridShaderProgram = glCreateProgram();
    glAttachShader(mGridShaderProgram, vs);
    glAttachShader(mGridShaderProgram, fs);
    glLinkProgram(mGridShaderProgram);
    GLint linked = 0;
    glGetProgramiv(mGridShaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(mGridShaderProgram, sizeof(log), nullptr, log);
        std::cerr << "Grid shader link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &mGridVao);
    glGenBuffers(1, &mGridVbo);
    glBindVertexArray(mGridVao);
    glBindBuffer(GL_ARRAY_BUFFER, mGridVbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    mGridRendererReady = true;
}

void MyGuiCanvas::teardownGridRenderer() {
    if (!mGridRendererReady) {
        return;
    }
    glDeleteProgram(mGridShaderProgram);
    glDeleteVertexArrays(1, &mGridVao);
    glDeleteBuffers(1, &mGridVbo);
    mGridShaderProgram = mGridVao = mGridVbo = 0;
    mGridRendererReady = false;
}

void MyGuiCanvas::renderBackdropAndGrid() {
    if (!mGridRendererReady) {
        return;
    }

    auto toNdc = [&](double screenX, double screenY) {
        return std::pair<float, float>{
            static_cast<float>((screenX / mViewWidth) * 2.0 - 1.0),
            static_cast<float>(1.0 - (screenY / mViewHeight) * 2.0)};
    };

    glUseProgram(mGridShaderProgram);
    GLint colorLoc = glGetUniformLocation(mGridShaderProgram, "uColor");
    glBindVertexArray(mGridVao);
    glBindBuffer(GL_ARRAY_BUFFER, mGridVbo);

    // The "screen" backdrop, so a letterboxed/pillarboxed margin reads as outside the screen.
    model::Rect topLeft = referenceToScreen({0, 0, 0, 0});
    model::Rect bottomRight = referenceToScreen({mReferenceWidth, mReferenceHeight, 0, 0});
    auto [x0, y0] = toNdc(topLeft.x, topLeft.y);
    auto [x1, y1] = toNdc(bottomRight.x, bottomRight.y);
    float quad[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    glUniform4f(colorLoc, 0.08f, 0.08f, 0.09f, 1.0f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Grid dots across the whole viewport, extending into the letterbox margin. Hidden when the grid is toggled off.
    if (mGridEnabled) {
        double minRefX, minRefY, maxRefX, maxRefY;
        screenToReference(0, 0, minRefX, minRefY);
        screenToReference(mViewWidth, mViewHeight, maxRefX, maxRefY);
        double startX = std::floor(minRefX / mGridSpacing) * mGridSpacing;
        double startY = std::floor(minRefY / mGridSpacing) * mGridSpacing;

        std::vector<float> points;
        for (double gx = startX; gx <= maxRefX + 0.5; gx += mGridSpacing) {
            for (double gy = startY; gy <= maxRefY + 0.5; gy += mGridSpacing) {
                model::Rect p = referenceToScreen({gx, gy, 0, 0});
                auto [nx, ny] = toNdc(p.x, p.y);
                points.push_back(nx);
                points.push_back(ny);
            }
        }
        glBufferData(
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points.size() * sizeof(float)), points.data(), GL_DYNAMIC_DRAW);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 0.15f);
        glPointSize(2.0f);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points.size() / 2));
        glDisable(GL_BLEND);
    }

    // Alignment guide lines: full-viewport line at whatever candidate the current drag tick snapped to.
    if (mShowSnapGuideX || mShowSnapGuideY) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform4f(colorLoc, 1.0f, 0.3f, 0.7f, 0.9f); // magenta-pink - distinct from the cyan selection outline
        if (mShowSnapGuideX) {
            model::Rect p = referenceToScreen({mSnapGuideX, 0, 0, 0});
            auto [nx0, ny0] = toNdc(p.x, 0);
            auto [nx1, ny1] = toNdc(p.x, mViewHeight);
            float line[4] = {nx0, ny0, nx1, ny1};
            glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, 2);
        }
        if (mShowSnapGuideY) {
            model::Rect p = referenceToScreen({0, mSnapGuideY, 0, 0});
            auto [nx0, ny0] = toNdc(0, p.y);
            auto [nx1, ny1] = toNdc(mViewWidth, p.y);
            float line[4] = {nx0, ny0, nx1, ny1};
            glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, 2);
        }
        glDisable(GL_BLEND);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void MyGuiCanvas::loadGameResources(const std::function<void(const std::string&, const std::string&, double)>& onStep) {
    auto* gui = new MyGUI::Gui();
    gui->initialise(""); // skip auto core-file loading; we replicate GuiConfig.xml's load order ourselves
    mGuiInitialised = true;

    mAvailableSkins.clear();
    std::string guiConfigText = readFile(mAssetsRoot + "/GuiConfig.xml");
    std::vector<std::string> entries = parseListFiles(guiConfigText);
    const size_t totalSteps = entries.size() + 2; // +2 for the fixed editor-fonts.xml/editor-skins.xml steps below
    size_t stepIndex = 0;
    for (const std::string& entry : entries) {
        std::string relative = relativePath(entry);
        std::string fullPath = mAssetsRoot + "/" + relative;
        if (onStep) {
            onStep("Loading skin/resource XML", relative, static_cast<double>(stepIndex) / totalSteps);
        }
        ++stepIndex;
        MyGUI::ResourceManager::getInstance().load(basename(entry));
        try {
            std::string category = basename(relative);
            if (size_t dot = category.rfind('.'); dot != std::string::npos) {
                category = category.substr(0, dot);
            }
            collectSkinInfos(readFile(fullPath), category, parentDir(fullPath), mAvailableSkins);
        } catch (const std::exception&) {
            // Not every listed resource is skin XML - a genuinely unreadable file shouldn't abort the load.
        }
    }
    // The game's own Fonts.xml has huge CJK atlases we don't need; editor-fonts.xml has just the Latin fonts in use.
    if (onStep) {
        onStep("Loading fonts...", "editor-fonts.xml", static_cast<double>(stepIndex) / totalSteps);
    }
    ++stepIndex;
    MyGUI::ResourceManager::getInstance().load("editor-fonts.xml");

    if (onStep) {
        onStep("Loading MyGUI skins...", "editor-skins.xml", static_cast<double>(stepIndex) / totalSteps);
    }
    ++stepIndex;
    MyGUI::ResourceManager::getInstance().load("editor-skins.xml");
    collectSkinInfos(
        readFile(parentDir(mAssetsRoot) + "/editor-skins.xml"), "editor-skins", parentDir(mAssetsRoot),
        mAvailableSkins);

    std::sort(mAvailableSkins.begin(), mAvailableSkins.end(), [](const SkinInfo& a, const SkinInfo& b) {
        return a.name < b.name;
    });
    mAvailableSkins.erase(
        std::unique(mAvailableSkins.begin(), mAvailableSkins.end(), [](const SkinInfo& a, const SkinInfo& b) {
            return a.name == b.name;
        }),
        mAvailableSkins.end());
}

void MyGuiCanvas::rebuildWidgets() {
    destroyOverlay();
    destroyWidgets();
    mFlatRects.clear();
    if (!mGuiInitialised || !mRoots) {
        return;
    }

    // Layout math happens entirely in reference space - only MyGUI widget coordinates get mapped to screen pixels.
    model::Rect canvasRect{0, 0, mReferenceWidth, mReferenceHeight};

    std::function<void(model::WidgetNode&, MyGUI::Widget*, const model::Rect&)> build =
        [&](model::WidgetNode& node, MyGUI::Widget* parent, const model::Rect& parentRect) {
            model::Rect rect = node.resolveRect(parentRect); // absolute, reference space
            std::string skin = resolveSkin(node.skin());

            MyGUI::Widget* widget = nullptr;
            if (parent) {
                // MyGUI child widget coords are parent-relative, unlike resolveRect()'s absolute rects.
                double scale = viewScale();
                MyGUI::IntCoord relative(
                    static_cast<int>(std::lround((rect.x - parentRect.x) * scale)),
                    static_cast<int>(std::lround((rect.y - parentRect.y) * scale)),
                    std::max(1, static_cast<int>(std::lround(rect.width * scale))),
                    std::max(1, static_cast<int>(std::lround(rect.height * scale))));
                widget = parent->createWidget<MyGUI::Widget>(skin, relative, MyGUI::Align::Default);
            } else {
                widget = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
                    skin, toIntCoord(referenceToScreen(rect)), MyGUI::Align::Default, "LuaHudOverlapped");
                if (widget) {
                    mTopLevelWidgets.push_back(widget);
                }
            }

            // createWidget can still return null if resources failed to load - don't crash the whole rebuild.
            if (!widget) {
                std::cerr << "Failed to create widget for '" << node.name << "'\n";
                return;
            }

            if (skin == kFallbackSkin) {
                // Neutral gray so a placeholder doesn't read as deliberate white content.
                widget->setColour(MyGUI::Colour(0.55f, 0.55f, 0.58f));
            }

            mFlatRects.push_back({&node, rect, parentRect, widget});

            for (model::WidgetNode& child : node.children) {
                build(child, widget, rect);
            }
        };

    // Creation order determines MyGUI's stacking order (later = on top) - no separate z-order property to sync.
    for (model::WidgetNode& root : *mRoots) {
        build(root, nullptr, canvasRect);
    }

    rebuildOverlay();
}

void MyGuiCanvas::destroyWidgets() {
    if (mGuiInitialised) {
        for (MyGUI::Widget* widget : mTopLevelWidgets) {
            MyGUI::Gui::getInstance().destroyWidget(widget);
        }
    }
    mTopLevelWidgets.clear();
}

void MyGuiCanvas::destroyOverlay() {
    if (mGuiInitialised) {
        for (MyGUI::Widget* widget : mOverlayWidgets) {
            MyGUI::Gui::getInstance().destroyWidget(widget);
        }
    }
    mOverlayWidgets.clear();
}

void MyGuiCanvas::rebuildOverlay() {
    destroyOverlay();
    if (!mGuiInitialised || !mSelected || !rectFor(mSelected)) {
        return;
    }

    auto createPiece = [&](const model::Rect& referenceRect, const MyGUI::Colour& colour) {
        MyGUI::Widget* piece = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
            kFallbackSkin, toIntCoord(referenceToScreen(referenceRect)), MyGUI::Align::Default, "LuaHudOverlapped");
        if (piece) {
            piece->setColour(colour);
            mOverlayWidgets.push_back(piece);
        }
    };

    // Thin outline around the selected widget's bounds, bright enough for light and dark skins.
    const model::Rect selectedRect = *rectFor(mSelected);
    const MyGUI::Colour kOutlineColour(0.3f, 0.75f, 1.0f);
    double borderThickness = 2.0 / viewScale(); // reference units -> a fixed 2 screen px
    createPiece({selectedRect.x, selectedRect.y, selectedRect.width, borderThickness}, kOutlineColour); // top
    createPiece(
        {selectedRect.x, selectedRect.y + selectedRect.height - borderThickness, selectedRect.width, borderThickness},
        kOutlineColour); // bottom
    createPiece({selectedRect.x, selectedRect.y, borderThickness, selectedRect.height}, kOutlineColour); // left
    createPiece(
        {selectedRect.x + selectedRect.width - borderThickness, selectedRect.y, borderThickness, selectedRect.height},
        kOutlineColour); // right

    // Each corner gets a distinct colour so its resize direction is obvious; anchor gets its own.
    createPiece(handleRect(HandleKind::TopLeft), MyGUI::Colour(0.95f, 0.25f, 0.25f));
    createPiece(handleRect(HandleKind::TopRight), MyGUI::Colour(0.25f, 0.85f, 0.35f));
    createPiece(handleRect(HandleKind::BottomLeft), MyGUI::Colour(0.3f, 0.55f, 0.95f));
    createPiece(handleRect(HandleKind::BottomRight), MyGUI::Colour(0.95f, 0.85f, 0.25f));
    createPiece(handleRect(HandleKind::Anchor), MyGUI::Colour(0.95f, 0.95f, 0.95f));
}

const model::Rect* MyGuiCanvas::rectFor(model::WidgetNode* node) const {
    if (!node) {
        return nullptr;
    }
    for (const FlatEntry& entry : mFlatRects) {
        if (entry.node == node) {
            return &entry.rect;
        }
    }
    return nullptr;
}

model::Rect MyGuiCanvas::handleRect(HandleKind kind) const {
    const model::Rect* selectedRect = rectFor(mSelected);
    model::Rect rect = selectedRect ? *selectedRect : model::Rect{};

    double centerX = rect.x;
    double centerY = rect.y;
    switch (kind) {
        case HandleKind::TopLeft:
            centerX = rect.x;
            centerY = rect.y;
            break;
        case HandleKind::TopRight:
            centerX = rect.x + rect.width;
            centerY = rect.y;
            break;
        case HandleKind::BottomLeft:
            centerX = rect.x;
            centerY = rect.y + rect.height;
            break;
        case HandleKind::BottomRight:
            centerX = rect.x + rect.width;
            centerY = rect.y + rect.height;
            break;
        case HandleKind::Anchor: {
            model::Anchor anchor = mSelected ? mSelected->anchor() : model::Anchor{};
            centerX = rect.x + anchor.x * rect.width;
            centerY = rect.y + anchor.y * rect.height;
            break;
        }
    }
    // Reference units that resolve to a fixed kHandleSize screen pixels, so handles don't scale with zoom.
    double refHandleSize = kHandleSize / viewScale();
    return model::Rect{centerX - refHandleSize / 2, centerY - refHandleSize / 2, refHandleSize, refHandleSize};
}

double MyGuiCanvas::viewScale() const {
    if (mViewWidth <= 0 || mViewHeight <= 0) {
        return 1.0;
    }
    return std::min(mViewWidth / mReferenceWidth, mViewHeight / mReferenceHeight);
}

model::Rect MyGuiCanvas::referenceToScreen(const model::Rect& rect) const {
    double scale = viewScale();
    double offsetX = (mViewWidth - mReferenceWidth * scale) / 2.0;
    double offsetY = (mViewHeight - mReferenceHeight * scale) / 2.0;
    return model::Rect{rect.x * scale + offsetX, rect.y * scale + offsetY, rect.width * scale, rect.height * scale};
}

void MyGuiCanvas::screenToReference(double screenX, double screenY, double& refX, double& refY) const {
    double scale = viewScale();
    double offsetX = (mViewWidth - mReferenceWidth * scale) / 2.0;
    double offsetY = (mViewHeight - mReferenceHeight * scale) / 2.0;
    refX = (screenX - offsetX) / scale;
    refY = (screenY - offsetY) / scale;
}

model::WidgetNode* MyGuiCanvas::hitTestBody(double x, double y) const {
    for (auto it = mFlatRects.rbegin(); it != mFlatRects.rend(); ++it) {
        const model::Rect& r = it->rect;
        if (x >= r.x && x <= r.x + r.width && y >= r.y && y <= r.y + r.height) {
            return it->node;
        }
    }
    return nullptr;
}

MyGuiCanvas::DragMode MyGuiCanvas::hitTestHandle(double x, double y) const {
    if (!mSelected || !rectFor(mSelected)) {
        return DragMode::None;
    }
    auto contains = [&](const model::Rect& r) { return x >= r.x && x <= r.x + r.width && y >= r.y && y <= r.y + r.height; };
    if (contains(handleRect(HandleKind::TopLeft))) {
        return DragMode::ResizeTopLeft;
    }
    if (contains(handleRect(HandleKind::TopRight))) {
        return DragMode::ResizeTopRight;
    }
    if (contains(handleRect(HandleKind::BottomLeft))) {
        return DragMode::ResizeBottomLeft;
    }
    if (contains(handleRect(HandleKind::BottomRight))) {
        return DragMode::ResizeBottomRight;
    }
    if (contains(handleRect(HandleKind::Anchor))) {
        return DragMode::Anchor;
    }
    return DragMode::None;
}

void MyGuiCanvas::collectSnapCandidates(model::WidgetNode* exclude, std::vector<double>& xs, std::vector<double>& ys)
    const {
    xs = {0.0, mReferenceWidth / 2.0, mReferenceWidth};
    ys = {0.0, mReferenceHeight / 2.0, mReferenceHeight};
    for (const FlatEntry& entry : mFlatRects) {
        if (entry.node == exclude) {
            continue;
        }
        xs.push_back(entry.rect.x);
        xs.push_back(entry.rect.x + entry.rect.width / 2.0);
        xs.push_back(entry.rect.x + entry.rect.width);
        ys.push_back(entry.rect.y);
        ys.push_back(entry.rect.y + entry.rect.height / 2.0);
        ys.push_back(entry.rect.y + entry.rect.height);
    }
}

double MyGuiCanvas::snapOffset(
    double start, double size, const std::vector<double>& candidates, double* outCandidateValue) const {
    double bestOffset = 0.0;
    double bestDelta = kSnapThreshold;
    double bestCandidate = 0.0;
    double edges[3] = {start, start + size / 2.0, start + size};
    for (double candidate : candidates) {
        for (double edge : edges) {
            double delta = candidate - edge;
            if (std::abs(delta) < bestDelta) {
                bestDelta = std::abs(delta);
                bestOffset = delta;
                bestCandidate = candidate;
            }
        }
    }
    if (outCandidateValue) {
        *outCandidateValue = bestCandidate;
    }
    return bestOffset;
}

double MyGuiCanvas::stickySnapOffset(
    StickySnapAxis& axis, double start, double size, const std::vector<double>& candidates,
    double* outCandidateValue) const {
    if (axis.active) {
        double bestDelta = kUnsnapThreshold;
        double bestOffset = 0.0;
        bool stillClose = false;
        double edges[3] = {start, start + size / 2.0, start + size};
        for (double edge : edges) {
            double delta = axis.value - edge;
            if (std::abs(delta) < bestDelta) {
                bestDelta = std::abs(delta);
                bestOffset = delta;
                stillClose = true;
            }
        }
        if (stillClose) {
            if (outCandidateValue) {
                *outCandidateValue = axis.value;
            }
            return bestOffset;
        }
        axis.active = false; // moved far enough away - release, fall through to a fresh match below
    }

    double candidateValue = 0.0;
    double offset = snapOffset(start, size, candidates, &candidateValue);
    if (offset != 0.0) {
        axis.active = true;
        axis.value = candidateValue;
    }
    if (outCandidateValue) {
        *outCandidateValue = candidateValue;
    }
    return offset;
}

double MyGuiCanvas::snapSingleValue(double value, const std::vector<double>& candidates, bool* outMatched) const {
    double best = value;
    double bestDelta = kSnapThreshold;
    bool matched = false;
    for (double candidate : candidates) {
        double delta = std::abs(value - candidate);
        if (delta < bestDelta) {
            bestDelta = delta;
            best = candidate;
            matched = true;
        }
    }
    if (outMatched) {
        *outMatched = matched;
    }
    if (matched) {
        return best;
    }
    return mGridEnabled ? std::round(value / mGridSpacing) * mGridSpacing : value;
}

void MyGuiCanvas::onDragBegin(double screenX, double screenY) {
    gtk_widget_grab_focus(mGlArea);

    double x, y;
    screenToReference(screenX, screenY, x, y);

    DragMode mode = hitTestHandle(x, y);
    model::WidgetNode* target = mSelected;

    if (mode == DragMode::None) {
        // A click within the currently selected widget's box keeps manipulating that
        // selection instead of drilling into a child on top of it, until deselected.
        const model::Rect* selectedRect = mSelected ? rectFor(mSelected) : nullptr;
        model::WidgetNode* hit = (selectedRect && x >= selectedRect->x && x <= selectedRect->x + selectedRect->width &&
                                   y >= selectedRect->y && y <= selectedRect->y + selectedRect->height)
                                      ? mSelected
                                      : hitTestBody(x, y);
        if (hit != mSelected) {
            setSelection(hit);
            if (mOnSelectionChanged) {
                mOnSelectionChanged(hit);
            }
        }
        target = hit;
        mode = hit ? DragMode::Move : DragMode::None;
    }

    mDragMode = mode;
    mDragNode = target;
    mDragStartMouseX = x; // reference space, so offsets below need dividing by viewScale()
    mDragStartMouseY = y;
    mSnapStickyX = StickySnapAxis{};
    mSnapStickyY = StickySnapAxis{};
    mShowSnapGuideX = false;
    mShowSnapGuideY = false;

    if (mDragNode) {
        mDragStartProperties = mDragNode->properties;
        for (const FlatEntry& entry : mFlatRects) {
            if (entry.node == mDragNode) {
                mDragStartRect = entry.rect;
                mDragParentRect = entry.parentRect;
                break;
            }
        }
    }
}

void MyGuiCanvas::onDragUpdate(double screenOffsetX, double screenOffsetY) {
    if (!mDragNode || mDragMode == DragMode::None) {
        return;
    }

    gtk_gl_area_make_current(GTK_GL_AREA(mGlArea));
    mDragNode->properties = mDragStartProperties; // reapply from a clean snapshot each tick

    // GtkGestureDrag reports offsets in real screen pixels; everything else
    // here (positions, deltas, node properties) is reference space.
    double scale = viewScale();
    double offsetX = screenOffsetX / scale;
    double offsetY = screenOffsetY / scale;
    double mouseX = mDragStartMouseX + offsetX;
    double mouseY = mDragStartMouseY + offsetY;

    std::vector<double> snapXs, snapYs;
    collectSnapCandidates(mDragNode, snapXs, snapYs);
    mShowSnapGuideX = false;
    mShowSnapGuideY = false;

    switch (mDragMode) {
        case DragMode::Move: {
            // Snaps via the nearest edge/center to a candidate, sticky once engaged; falls back to grid-lock.
            double proposedX = mDragStartRect.x + offsetX;
            double proposedY = mDragStartRect.y + offsetY;
            double snapCandidateX = 0.0, snapCandidateY = 0.0;
            double snapDx = stickySnapOffset(mSnapStickyX, proposedX, mDragStartRect.width, snapXs, &snapCandidateX);
            double snapDy = stickySnapOffset(mSnapStickyY, proposedY, mDragStartRect.height, snapYs, &snapCandidateY);
            mShowSnapGuideX = snapDx != 0.0;
            mShowSnapGuideY = snapDy != 0.0;
            mSnapGuideX = snapCandidateX;
            mSnapGuideY = snapCandidateY;
            std::cerr << "[snapdbg] proposed=(" << proposedX << "," << proposedY << ") mid=("
                      << (proposedX + mDragStartRect.width / 2.0) << "," << (proposedY + mDragStartRect.height / 2.0)
                      << ") snapDx=" << snapDx << " snapDy=" << snapDy << " cand=(" << snapCandidateX << ","
                      << snapCandidateY << ")"
                      << " xs=[";
            for (double v : snapXs) std::cerr << v << " ";
            std::cerr << "] ys=[";
            for (double v : snapYs) std::cerr << v << " ";
            std::cerr << "]\n";
            double gridFallbackX = mGridEnabled ? (std::round(proposedX / mGridSpacing) * mGridSpacing - proposedX) : 0.0;
            double gridFallbackY = mGridEnabled ? (std::round(proposedY / mGridSpacing) * mGridSpacing - proposedY) : 0.0;
            proposedX += mShowSnapGuideX ? snapDx : gridFallbackX;
            proposedY += mShowSnapGuideY ? snapDy : gridFallbackY;

            // rect.x/rect.y move 1:1 with x.pixel/y.pixel regardless of anchor during a move (size is unchanged).
            model::LayoutAxis x = mDragNode->x();
            model::LayoutAxis y = mDragNode->y();
            x.pixel += proposedX - mDragStartRect.x;
            y.pixel += proposedY - mDragStartRect.y;
            mDragNode->setX(x);
            mDragNode->setY(y);
            break;
        }
        case DragMode::ResizeTopLeft:
        case DragMode::ResizeTopRight:
        case DragMode::ResizeBottomLeft:
        case DragMode::ResizeBottomRight: {
            bool isLeft = mDragMode == DragMode::ResizeTopLeft || mDragMode == DragMode::ResizeBottomLeft;
            bool isTop = mDragMode == DragMode::ResizeTopLeft || mDragMode == DragMode::ResizeTopRight;

            // Only the dragged corner moves, so only it needs to snap.
            bool matchedX = false, matchedY = false;
            mouseX = snapSingleValue(mouseX, snapXs, &matchedX);
            mouseY = snapSingleValue(mouseY, snapYs, &matchedY);
            mShowSnapGuideX = matchedX;
            mShowSnapGuideY = matchedY;
            mSnapGuideX = mouseX;
            mSnapGuideY = mouseY;

            // Solve the new absolute rect first, then back-compute pixel
            // offsets - keeps the opposite corner fixed regardless of anchor.
            // Clamped with max(), not abs(): abs() would flip the delta back
            // positive once the mouse crosses the fixed corner, growing the
            // wrong way instead of stopping at the minimum size.
            constexpr double kMinSize = 4.0;
            double fixedX = isLeft ? (mDragStartRect.x + mDragStartRect.width) : mDragStartRect.x;
            double fixedY = isTop ? (mDragStartRect.y + mDragStartRect.height) : mDragStartRect.y;
            double newWidth = std::max(kMinSize, isLeft ? (fixedX - mouseX) : (mouseX - fixedX));
            double newHeight = std::max(kMinSize, isTop ? (fixedY - mouseY) : (mouseY - fixedY));
            double newRectX = isLeft ? (fixedX - newWidth) : fixedX;
            double newRectY = isTop ? (fixedY - newHeight) : fixedY;

            model::Anchor anchor = mDragNode->anchor();
            model::LayoutAxis x = mDragNode->x();
            model::LayoutAxis y = mDragNode->y();
            model::LayoutAxis w = mDragNode->width();
            model::LayoutAxis h = mDragNode->height();

            w.pixel = newWidth - mDragParentRect.width * w.scale;
            h.pixel = newHeight - mDragParentRect.height * h.scale;
            x.pixel = (newRectX + anchor.x * newWidth) - mDragParentRect.x - mDragParentRect.width * x.scale;
            y.pixel = (newRectY + anchor.y * newHeight) - mDragParentRect.y - mDragParentRect.height * y.scale;

            mDragNode->setX(x);
            mDragNode->setY(y);
            mDragNode->setWidth(w);
            mDragNode->setHeight(h);
            break;
        }
        case DragMode::Anchor: {
            // Dragging the marker keeps the box where it visually is, changing which point sits on the anchor -
            // solve x.pixel/y.pixel backwards to compensate.
            double newAnchorX = mDragStartRect.width != 0 ? (mouseX - mDragStartRect.x) / mDragStartRect.width : 0.0;
            double newAnchorY = mDragStartRect.height != 0 ? (mouseY - mDragStartRect.y) / mDragStartRect.height : 0.0;
            newAnchorX = std::clamp(newAnchorX, 0.0, 1.0);
            newAnchorY = std::clamp(newAnchorY, 0.0, 1.0);

            double anchorAbsX = mDragStartRect.x + newAnchorX * mDragStartRect.width;
            double anchorAbsY = mDragStartRect.y + newAnchorY * mDragStartRect.height;

            model::LayoutAxis x = mDragNode->x();
            model::LayoutAxis y = mDragNode->y();
            x.pixel = anchorAbsX - mDragParentRect.x - mDragParentRect.width * x.scale;
            y.pixel = anchorAbsY - mDragParentRect.y - mDragParentRect.height * y.scale;

            mDragNode->setAnchor({newAnchorX, newAnchorY});
            mDragNode->setX(x);
            mDragNode->setY(y);
            break;
        }
        case DragMode::None:
            break;
    }

    updateDraggedWidgetGeometry();
    gtk_widget_queue_draw(mGlArea);

    if (mOnDragUpdate) {
        mOnDragUpdate();
    }
}

void MyGuiCanvas::updateDraggedWidgetGeometry() {
    if (!mDragNode) {
        return;
    }

    // Recomputes cached absolute rects for mDragNode and descendants;
    // descendant widgets already moved visually (parent-relative coords),
    // only mFlatRects' bookkeeping needs to catch up.
    std::function<void(model::WidgetNode&, const model::Rect&)> updateRect =
        [&](model::WidgetNode& node, const model::Rect& parentRect) {
            model::Rect rect = node.resolveRect(parentRect);
            for (FlatEntry& entry : mFlatRects) {
                if (entry.node == &node) {
                    entry.rect = rect;
                    entry.parentRect = parentRect;
                    break;
                }
            }
            for (model::WidgetNode& child : node.children) {
                updateRect(child, rect);
            }
        };
    updateRect(*mDragNode, mDragParentRect);

    FlatEntry* draggedEntry = nullptr;
    for (FlatEntry& entry : mFlatRects) {
        if (entry.node == mDragNode) {
            draggedEntry = &entry;
            break;
        }
    }
    if (draggedEntry && draggedEntry->widget) {
        bool isChild = model::findParentOf(*mRoots, mDragNode) != nullptr;
        if (isChild) {
            // Mirrors rebuildWidgets()'s child branch: MyGUI child coords are parent-relative.
            double scale = viewScale();
            MyGUI::IntCoord relative(
                static_cast<int>(std::lround((draggedEntry->rect.x - mDragParentRect.x) * scale)),
                static_cast<int>(std::lround((draggedEntry->rect.y - mDragParentRect.y) * scale)),
                std::max(1, static_cast<int>(std::lround(draggedEntry->rect.width * scale))),
                std::max(1, static_cast<int>(std::lround(draggedEntry->rect.height * scale))));
            draggedEntry->widget->setCoord(relative);
        } else {
            draggedEntry->widget->setCoord(toIntCoord(referenceToScreen(draggedEntry->rect)));
        }
    }

    rebuildOverlay();
}

void MyGuiCanvas::onDragEnd(double, double) {
    if (mDragNode && mUndoStack && mDragStartProperties != mDragNode->properties) {
        model::WidgetNode* node = mDragNode;
        format::JsonValue before = mDragStartProperties;
        format::JsonValue after = node->properties;
        MyGuiCanvas* self = this;
        mUndoStack->push(
            [self, node, before]() {
                node->properties = before;
                gtk_gl_area_make_current(GTK_GL_AREA(self->mGlArea));
                self->rebuildWidgets();
                gtk_widget_queue_draw(self->mGlArea);
                if (self->mOnDragEnd) {
                    self->mOnDragEnd();
                }
            },
            [self, node, after]() {
                node->properties = after;
                gtk_gl_area_make_current(GTK_GL_AREA(self->mGlArea));
                self->rebuildWidgets();
                gtk_widget_queue_draw(self->mGlArea);
                if (self->mOnDragEnd) {
                    self->mOnDragEnd();
                }
            });
    }
    mDragMode = DragMode::None;
    mDragNode = nullptr;
    mShowSnapGuideX = false;
    mShowSnapGuideY = false;
    gtk_widget_queue_draw(mGlArea); // clears the alignment guide lines
    if (mOnDragEnd) {
        mOnDragEnd();
    }
}

void MyGuiCanvas::onRightClick(double screenX, double screenY) {
    gtk_widget_grab_focus(mGlArea);
    double x, y;
    screenToReference(screenX, screenY, x, y);
    model::WidgetNode* hit = hitTestBody(x, y);
    if (mOnContextMenuRequested) {
        mOnContextMenuRequested(hit, screenX, screenY);
    }
}

void MyGuiCanvas::realizeThunk(GtkGLArea*, gpointer userData) { static_cast<MyGuiCanvas*>(userData)->onRealize(); }

void MyGuiCanvas::unrealizeThunk(GtkGLArea*, gpointer userData) { static_cast<MyGuiCanvas*>(userData)->onUnrealize(); }

void MyGuiCanvas::resizeThunk(GtkGLArea*, int width, int height, gpointer userData) {
    static_cast<MyGuiCanvas*>(userData)->onResize(width, height);
}

gboolean MyGuiCanvas::renderThunk(GtkGLArea*, GdkGLContext*, gpointer userData) {
    static_cast<MyGuiCanvas*>(userData)->onRender();
    return TRUE;
}

void MyGuiCanvas::dragBeginThunk(GtkGestureDrag*, double x, double y, gpointer userData) {
    static_cast<MyGuiCanvas*>(userData)->onDragBegin(x, y);
}

void MyGuiCanvas::dragUpdateThunk(GtkGestureDrag*, double offsetX, double offsetY, gpointer userData) {
    static_cast<MyGuiCanvas*>(userData)->onDragUpdate(offsetX, offsetY);
}

void MyGuiCanvas::dragEndThunk(GtkGestureDrag*, double offsetX, double offsetY, gpointer userData) {
    static_cast<MyGuiCanvas*>(userData)->onDragEnd(offsetX, offsetY);
}

void MyGuiCanvas::rightClickThunk(GtkGestureClick*, gint, gdouble x, gdouble y, gpointer userData) {
    static_cast<MyGuiCanvas*>(userData)->onRightClick(x, y);
}

} // namespace regui::canvas
