#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "format/json_value.h"
#include "model/layout.h"
#include "model/undo_stack.h"

namespace MyGUI {
class OpenGL3Platform;
class Widget;
} // namespace MyGUI

namespace regui::canvas {

// How a SkinPiece's placement responds at a size other than the skin's
// native one - mirrors a BasisSkin's `align` attribute, per axis.
enum class SkinAlign { Begin, End, Stretch };

// One 9-sliced piece of a skin's "normal" state, from its XML: native
// `place`, `textureRect` sampled from the atlas, and per-axis align -
// enough to reproduce MyGUI's 9-slice at an arbitrary preview size.
struct SkinPiece {
    model::Rect place;
    model::Rect textureRect;
    SkinAlign hAlign = SkinAlign::Begin;
    SkinAlign vAlign = SkinAlign::Begin;
};

// Enough of a ResourceSkin to paint a static preview (see
// ui::SkinPickerDialog), not a full BasisSkin interpretation. Procedural
// skins (text/polygon/render-to-texture) are left with empty
// pieces/texturePath and the picker shows a placeholder instead.
struct SkinInfo {
    std::string name;
    std::string category; // the skin XML file's basename this came from
    double baseWidth = 0;
    double baseHeight = 0;
    std::string texturePath; // absolute path to the atlas PNG; empty if unsupported
    std::vector<SkinPiece> pieces;
};

// One update during MyGuiCanvas's one-time asset loading - see
// setOnLoadProgress(). `what`/`where`/`who` describe the current step;
// `fraction` (0..1) is progress through the whole sequence.
struct LoadProgress {
    std::string what;
    std::string where;
    std::string who;
    double fraction = 0.0;
};

// One resolution the game ships a dedicated GUI asset pack for (see
// guiResolutions.json's "resolutionList"). Only one of these can be loaded
// into MyGUI at a time - see setActiveGameResolution().
struct GameResolution {
    double width = 0;
    double height = 0;
    std::string folder; // relative to mAssetsRoot, e.g. "Resolutions/1920x1080"
};

// Owns the GtkGLArea, the MyGUI OpenGL3 platform, and a real MyGUI::Gui
// initialized against Scrap Mechanic's own skin/font resources. Also owns
// selection and click/drag editing of the widget tree it displays.
class MyGuiCanvas {
public:
    // `assetsRoot` is the local copy of the game's Data/Gui folder.
    explicit MyGuiCanvas(std::string assetsRoot);

    GtkWidget* widget() const { return mGlArea; }

    // The document must outlive the canvas, or a later call must clear this first.
    void setRoots(std::vector<model::WidgetNode>* roots);

    void setUndoStack(model::UndoStack* undoStack) { mUndoStack = undoStack; }

    // Programmatic selection; doesn't fire onSelectionChanged (avoids a feedback loop).
    void setSelection(model::WidgetNode* node);
    model::WidgetNode* selectedNode() const { return mSelected; }

    // Fired when the user changes selection by clicking (nullptr = empty space).
    void setOnSelectionChanged(std::function<void(model::WidgetNode*)> callback) {
        mOnSelectionChanged = std::move(callback);
    }

    // Re-resolves and redraws the current roots without touching selection.
    void refresh();

    // Fired after a drag (move/resize/anchor) completes.
    void setOnDragEnd(std::function<void()> callback) { mOnDragEnd = std::move(callback); }

    // Fired on every drag-update tick, after properties/canvas are already updated.
    void setOnDragUpdate(std::function<void()> callback) { mOnDragUpdate = std::move(callback); }

    // Simulated target resolution; widget properties stay in reference
    // units regardless. Defaults to 1920x1080. Triggers a rebuild/redraw.
    void setReferenceResolution(double width, double height);
    double referenceWidth() const { return mReferenceWidth; }
    double referenceHeight() const { return mReferenceHeight; }

    // Every resolution the game ships a dedicated GUI asset pack for (from
    // guiResolutions.json), in the order the file lists them. Empty until
    // the canvas realizes.
    const std::vector<GameResolution>& availableGameResolutions() const { return mGameResolutions; }
    int activeGameResolutionIndex() const { return mActiveResolutionIndex; }

    // Switches which resolution's GUI asset pack (Resolutions/<W>x<H>/...)
    // is loaded into MyGUI, using the same tiering the game itself uses
    // (GetMyGuiScreenSize()): the largest pack that still fits within
    // `width`/`height` on both axes, falling back to the smallest pack if
    // even that one doesn't fit. A no-op if that pack is already the active
    // one. Independent of setReferenceResolution's simulated canvas size -
    // callers typically want both. Reloads every MyGUI resource
    // (skins/fonts/etc.), so every widget on canvas is briefly torn down and
    // rebuilt; this is unavoidable since every resolution pack redefines the
    // same skin names over differently-sized textures, so only one can be
    // loaded at a time. `onProgress`, if given, is fired the same way
    // setOnLoadProgress()'s callback is during the initial load.
    void setActiveGameResolution(
        double width, double height, const std::function<void(const LoadProgress&)>& onProgress = {});

    // Grid dot spacing / hard-snap increment, reference units. Defaults to 10.
    void setGridSpacing(double spacing);
    double gridSpacing() const { return mGridSpacing; }

    // Toggles grid dots and grid-lock snap fallback; object/edge snapping is unaffected.
    void setGridEnabled(bool enabled);
    bool gridEnabled() const { return mGridEnabled; }

    // Every skin from the game's skin XML, for SkinPickerDialog. Empty
    // until the canvas realizes and loadGameResources() populates it.
    const std::vector<SkinInfo>& availableSkins() const { return mAvailableSkins; }

    // Fired repeatedly during the one-time asset load (see onRealize()).
    // Must be wired up before widget() realizes to see anything; runs
    // synchronously (single-threaded), so keep it cheap.
    void setOnLoadProgress(std::function<void(const LoadProgress&)> callback) {
        mOnLoadProgress = std::move(callback);
    }

    // Fired on right-click: `node` is the hit widget (nullptr = empty
    // space); coordinates are widget-local pixels for anchoring a popover.
    // Builds no menu itself - the caller shows whatever it likes.
    void setOnContextMenuRequested(std::function<void(model::WidgetNode* node, double screenX, double screenY)> callback) {
        mOnContextMenuRequested = std::move(callback);
    }

    // User-controlled view zoom, on top of the fixed reference-resolution
    // scale - 1.0 is "no extra zoom". Ctrl+scroll and the header's +/-
    // buttons drive this; it never touches widget data, only the view.
    double zoom() const { return mZoom; }
    void zoomIn() { zoomBy(kZoomStepFactor, mViewWidth / 2.0, mViewHeight / 2.0); }
    void zoomOut() { zoomBy(1.0 / kZoomStepFactor, mViewWidth / 2.0, mViewHeight / 2.0); }
    // Back to 1.0 zoom, no pan.
    void resetView();
    // Fired whenever zoom() changes, so the header can show a live percentage.
    void setOnZoomChanged(std::function<void(double)> callback) { mOnZoomChanged = std::move(callback); }

private:
    enum class HandleKind { TopLeft, TopRight, BottomLeft, BottomRight, Anchor };
    enum class DragMode { None, Move, ResizeTopLeft, ResizeTopRight, ResizeBottomLeft, ResizeBottomRight, Anchor };

    std::string mAssetsRoot;
    std::string mEditorAssetsRoot; // extracted embedded editor-fonts.xml/editor-skins.xml, set in initialisePlatform()
    double mReferenceWidth = 1920.0;
    double mReferenceHeight = 1080.0;
    double mGridSpacing = 10.0; // reference units
    bool mGridEnabled = true;
    std::vector<SkinInfo> mAvailableSkins; // populated by loadGameResources()
    std::vector<GameResolution> mGameResolutions; // from guiResolutions.json, parsed once
    std::vector<std::string> mResolutionResourceFiles; // guiResolutions.json's "resources" list, shared across packs
    int mActiveResolutionIndex = -1; // index into mGameResolutions currently loaded into MyGUI
    GtkWidget* mGlArea = nullptr;
    MyGUI::OpenGL3Platform* mPlatform = nullptr;
    class FreeImageLoader* mImageLoader = nullptr; // MyGUI doesn't take ownership; we do
    bool mGuiInitialised = false;
    std::vector<model::WidgetNode>* mRoots = nullptr;
    std::vector<MyGUI::Widget*> mTopLevelWidgets;
    std::vector<MyGUI::Widget*> mOverlayWidgets;
    int mViewWidth = 1;
    int mViewHeight = 1;
    // Frozen at the first real resize (and recomputed only when the
    // reference resolution itself changes) so widgets keep a fixed on-screen
    // size across window resizes/moves instead of growing/shrinking with them.
    double mFixedScale = 1.0;
    bool mScaleInitialized = false;

    // User pan/zoom, layered on top of mFixedScale - purely a view
    // transform, same as mFixedScale itself; never touches widget data.
    static constexpr double kZoomStepFactor = 1.25; // per scroll tick / button click
    static constexpr double kMinZoom = 0.1;
    static constexpr double kMaxZoom = 8.0;
    double mZoom = 1.0;
    double mPanX = 0.0; // screen pixels
    double mPanY = 0.0;
    double mPanDragStartX = 0.0;
    double mPanDragStartY = 0.0;
    double mLastPointerX = 0.0; // tracked via mMotionController, for scroll-to-zoom's anchor point
    double mLastPointerY = 0.0;
    std::function<void(double)> mOnZoomChanged;

    // Resolved rect per widget, depth-first, refreshed each rebuild - used
    // for hit-testing/drag math without walking MyGUI's own tree.
    struct FlatEntry {
        model::WidgetNode* node;
        model::Rect rect;
        model::Rect parentRect;
        MyGUI::Widget* widget = nullptr; // non-owning; see updateDraggedWidgetGeometry()
    };
    std::vector<FlatEntry> mFlatRects;

    model::WidgetNode* mSelected = nullptr;
    std::function<void(model::WidgetNode*)> mOnSelectionChanged;
    std::function<void()> mOnDragEnd;
    std::function<void()> mOnDragUpdate;
    std::function<void(const LoadProgress&)> mOnLoadProgress;
    std::function<void(model::WidgetNode*, double, double)> mOnContextMenuRequested;

    model::UndoStack* mUndoStack = nullptr;
    DragMode mDragMode = DragMode::None;
    model::WidgetNode* mDragNode = nullptr;
    format::JsonValue mDragStartProperties; // snapshot at drag-begin, for delta math and undo
    model::Rect mDragStartRect{};
    model::Rect mDragParentRect{};
    double mDragStartMouseX = 0;
    double mDragStartMouseY = 0;

    // One axis's "sticky" snap state across a Move drag - see stickySnapOffset().
    struct StickySnapAxis {
        bool active = false;
        double value = 0.0;
    };
    StickySnapAxis mSnapStickyX;
    StickySnapAxis mSnapStickyY;

    // Alignment guide lines: set while dragging when a tick's snap matched
    // a candidate, drawn full-viewport so it's visible why it snapped.
    bool mShowSnapGuideX = false;
    bool mShowSnapGuideY = false;
    double mSnapGuideX = 0.0; // reference space
    double mSnapGuideY = 0.0;

    // Raw-GL grid/backdrop, drawn directly (not as MyGUI widgets - too
    // expensive to recreate every drag tick) before MyGUI's own frame.
    bool mGridRendererReady = false;
    unsigned int mGridShaderProgram = 0;
    unsigned int mGridVao = 0;
    unsigned int mGridVbo = 0;

    void setupGridRenderer();
    void teardownGridRenderer();
    void renderBackdropAndGrid();

    // Snapping while dragging: object/screen-edge match within threshold
    // wins, else falls back to the grid if enabled.
    void collectSnapCandidates(model::WidgetNode* exclude, std::vector<double>& xs, std::vector<double>& ys) const;
    // Offset for `start` so one of {start, start+size/2, start+size}
    // aligns with the closest candidate within threshold; 0 if none match.
    double snapOffset(
        double start, double size, const std::vector<double>& candidates, double* outCandidateValue = nullptr) const;
    // Single moving edge version of snapOffset(), for Resize.
    double snapSingleValue(double value, const std::vector<double>& candidates, bool* outMatched = nullptr) const;
    // Sticky wrapper around snapOffset() for Move: keeps returning the
    // same candidate's offset until the widget moves past kUnsnapThreshold.
    double stickySnapOffset(
        StickySnapAxis& axis, double start, double size, const std::vector<double>& candidates,
        double* outCandidateValue) const;

    void onRealize();
    void onUnrealize();
    void onResize(int width, int height);
    void onRender();

    // Fires onStep(what, where, localFraction) as it goes - see setOnLoadProgress().
    // `desiredResolutionIndex` picks which GameResolution pack to load
    // alongside the core resources; -1 means "closest to the current
    // reference resolution" (used on first load, before any pack is active).
    void loadGameResources(
        const std::function<void(const std::string&, const std::string&, double)>& onStep,
        int desiredResolutionIndex = -1);
    void reportLoadProgress(const std::string& what, const std::string& where, const std::string& who, double fraction);
    // Creates mPlatform/mImageLoader and registers the resolution-independent
    // resource locations. Used both on first realize and when
    // setActiveGameResolution() tears down and rebuilds MyGUI.
    void initialisePlatform();
    // Parses guiResolutions.json into mGameResolutions/mResolutionResourceFiles. A no-op if already parsed.
    void parseGuiResolutions();
    // Largest GameResolution pack that still fits within (width, height) on
    // both axes (mirrors the game's own GetMyGuiScreenSize() tiering); the
    // smallest pack if none fit; -1 if none loaded.
    int findResolutionBucketIndex(double width, double height) const;
    // Registers mGameResolutions[index]'s folder and loads every resource
    // file guiResolutions.json lists that actually exists in it, appending
    // their skins to mAvailableSkins. Sets mActiveResolutionIndex.
    void loadResolutionResources(int index, const std::function<void(const std::string&, const std::string&, double)>& onStep);
    void rebuildWidgets();
    void destroyWidgets();
    void rebuildOverlay();
    void destroyOverlay();

    const model::Rect* rectFor(model::WidgetNode* node) const;
    model::Rect handleRect(HandleKind kind) const;
    model::WidgetNode* hitTestBody(double x, double y) const;
    DragMode hitTestHandle(double x, double y) const;

    // Fixed reference resolution regardless of window size - resizing the
    // editor only changes the view (letterbox offset), never the layout math
    // or the scale (see mFixedScale).
    double viewScale() const;
    void recomputeFixedScale();
    model::Rect referenceToScreen(const model::Rect& rect) const;
    void screenToReference(double screenX, double screenY, double& refX, double& refY) const;

    // Multiplies mZoom by `factor` (clamped to [kMinZoom, kMaxZoom]),
    // adjusting mPanX/mPanY so the reference-space point currently under
    // (anchorScreenX, anchorScreenY) stays under it on screen.
    void zoomBy(double factor, double anchorScreenX, double anchorScreenY);

    void onDragBegin(double x, double y);
    void onDragUpdate(double offsetX, double offsetY);
    void onDragEnd(double offsetX, double offsetY);
    // A history-view label/detail pair for the just-finished drag (mDragMode),
    // e.g. {"Widget Moved & Resized", "Moved from {X=0,Y=0} to {X=10,Y=0}, resized from ..."}.
    std::pair<std::string, std::string> describeDragChange(const model::Rect& before, const model::Rect& after) const;
    void onRightClick(double screenX, double screenY);
    void onMotion(double x, double y);
    gboolean onScroll(GtkEventControllerScroll* controller, double dx, double dy);
    void onPanDragBegin();
    void onPanDragUpdate(double offsetX, double offsetY);

    // Repositions/resizes just mDragNode's widget (children ride along via
    // MyGUI's own transform) instead of rebuilding the whole tree per tick.
    void updateDraggedWidgetGeometry();

    static void realizeThunk(GtkGLArea* area, gpointer userData);
    static void unrealizeThunk(GtkGLArea* area, gpointer userData);
    static void resizeThunk(GtkGLArea* area, int width, int height, gpointer userData);
    static gboolean renderThunk(GtkGLArea* area, GdkGLContext* context, gpointer userData);

    static void dragBeginThunk(GtkGestureDrag* gesture, double x, double y, gpointer userData);
    static void dragUpdateThunk(GtkGestureDrag* gesture, double offsetX, double offsetY, gpointer userData);
    static void dragEndThunk(GtkGestureDrag* gesture, double offsetX, double offsetY, gpointer userData);
    static void rightClickThunk(GtkGestureClick* gesture, gint nPress, gdouble x, gdouble y, gpointer userData);
    static void motionThunk(GtkEventControllerMotion* controller, double x, double y, gpointer userData);
    static gboolean scrollThunk(GtkEventControllerScroll* controller, double dx, double dy, gpointer userData);
    static void panDragBeginThunk(GtkGestureDrag* gesture, double x, double y, gpointer userData);
    static void panDragUpdateThunk(GtkGestureDrag* gesture, double offsetX, double offsetY, gpointer userData);
};

} // namespace regui::canvas
