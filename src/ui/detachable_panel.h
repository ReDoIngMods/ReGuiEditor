#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace regui::ui {

// Wraps a swappable content widget (e.g. a panel's widget()) with a small
// header (just a title - it's also the drag handle) so the whole thing can
// live either inside a dock slot (a GtkPaned child managed by the owner) or
// float in its own window that stays above the main window - via
// transient-for, not modal, so the main window stays fully interactive.
//
// This class only knows about "docked" (widget() sitting wherever the owner
// put it) vs. "floating" (its own window). It has no idea about edges/slots -
// the owner decides where widget() goes and drives setFloating() to move it
// in and out of a floating window; see MainWindow's drag-and-drop docking for
// the policy that actually picks a destination.
class DetachablePanel {
public:
    DetachablePanel(GtkWindow* mainWindow, std::string title);
    ~DetachablePanel();

    DetachablePanel(const DetachablePanel&) = delete;
    DetachablePanel& operator=(const DetachablePanel&) = delete;

    // Stable across setContent() calls and across dock/float moves - this is
    // what the owner places into a dock slot.
    GtkWidget* widget() const { return mWrapper; }

    // Swaps the actual panel content (e.g. after the owning panel object is
    // rebuilt from scratch). Safe regardless of whether widget() is
    // currently docked or floating.
    void setContent(GtkWidget* content);

    bool isFloating() const { return mFloating; }

    // true: widget() must already be unparented (the owner pulls it out of
    // its dock slot first) - creates the floating window and moves it in.
    // false: must currently be floating - destroys the floating window and
    // leaves widget() unparented again for the owner to dock somewhere.
    void setFloating(bool floating);

    // Fired when the floating window's own close button is used - the owner
    // decides where that redocks to (its last dock edge) and calls
    // setFloating(false) + re-parents widget() accordingly.
    void setOnFloatCloseRequested(std::function<void()> callback) { mOnFloatCloseRequested = std::move(callback); }

    // Geometry the floating window should open at the next time it floats
    // (Win32 only; ignored elsewhere) - meant for restoring a saved layout on
    // startup. Superseded the moment the user moves/resizes it while floating.
    void setNextFloatGeometry(int x, int y, int width, int height);

    // Current floating geometry if floating right now (live query), else
    // wherever it was last positioned before it last stopped floating (which
    // is also where it will reopen next). X/Y are -1 if never known (Win32 only).
    void currentFloatGeometry(int& x, int& y, int& width, int& height) const;

    // Attaches a GTK drag source to the header so this panel can be dragged
    // onto MainWindow's dock-zone overlay. Stashes `this` on the GdkDrag
    // (via g_object_set_data) for the drop side to read back and identify
    // which panel is being dragged.
    void enableDragSource();

private:
    GtkWindow* mMainWindow;
    std::string mTitle;
    GtkWidget* mWrapper;
    GtkWidget* mHeader;
    GtkWidget* mContent = nullptr;
    GtkWidget* mFloatWindow = nullptr;
    bool mFloating = false;

    std::function<void()> mOnFloatCloseRequested;

    // Where the floating window last was (or, before the first float, where
    // a restored layout says it should open) - doubles as "next open
    // position" so floating again later in the same session reopens in place.
    // Refreshed live from the OS window whenever currently floating.
    mutable int mFloatX = -1;
    mutable int mFloatY = -1;
    mutable int mFloatW = 340;
    mutable int mFloatH = 480;

    void captureFloatGeometry() const;
    static GdkContentProvider* onDragPrepare(GtkDragSource* source, double x, double y, gpointer userData);
    static void onDragBegin(GtkDragSource* source, GdkDrag* drag, gpointer userData);
    static gboolean onFloatCloseRequest(GtkWindow*, gpointer userData);
};

} // namespace regui::ui
