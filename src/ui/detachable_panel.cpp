#include "ui/detachable_panel.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WIN32
#include <gdk/win32/gdkwin32.h>
#endif

#include <utility>

namespace regui::ui {

namespace {

#ifdef GDK_WINDOWING_WIN32
HWND hwndFor(GtkWindow* window) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (!surface || !GDK_IS_WIN32_SURFACE(surface)) {
        return nullptr;
    }
    return gdk_win32_surface_get_handle(surface);
}
#endif

// Content type is never actually read back - the real payload is the panel
// pointer stashed on the GdkDrag itself (see onDragBegin) - this string just
// satisfies GTK's drag/drop type-negotiation plumbing.
constexpr const char* kDockDragMimeContent = "regui-dock-panel";

} // namespace

DetachablePanel::DetachablePanel(GtkWindow* mainWindow, std::string title) : mMainWindow(mainWindow), mTitle(std::move(title)) {
    mWrapper = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // mWrapper moves between dock slots and the floating window across its
    // life; whichever one currently holds it owns the only GTK reference, so
    // without this an unparent step (mid-move) would free it before it could
    // be moved to its next parent. This keeps it alive independently.
    g_object_ref_sink(mWrapper);

    mHeader = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    // Generous on all 4 sides: whichever edge this panel docks to, the
    // adjacent GtkPaned resize handle sits flush against mWrapper's outer
    // edge, and its grab area is a few pixels wider than its visible line.
    // A slim margin let that overlap the header, so dragging near the
    // boundary would randomly resize the paned instead of (or as well as)
    // starting a redock drag - this keeps clear water between the two.
    gtk_widget_set_margin_start(mHeader, 10);
    gtk_widget_set_margin_end(mHeader, 10);
    gtk_widget_set_margin_top(mHeader, 10);
    gtk_widget_set_margin_bottom(mHeader, 10);
    gtk_widget_add_css_class(mHeader, "regui-dock-header");

    GtkWidget* label = gtk_label_new(mTitle.c_str());
    gtk_widget_add_css_class(label, "heading");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(mHeader), label);
    gtk_widget_set_tooltip_text(mHeader, "Drag to dock at an edge, or drop in the middle to float");
    // "grab"/"grabbing" silently resolve to a blank cursor on GDK's Win32
    // backend instead of falling back to a visible one - "pointer" is
    // universally supported and still reads as "this is interactive".
    gtk_widget_set_cursor_from_name(mHeader, "pointer");

    gtk_box_append(GTK_BOX(mWrapper), mHeader);
}

DetachablePanel::~DetachablePanel() {
    if (mFloatWindow) {
        // Pull the wrapper (and its still-referenced content) out first - the
        // window would otherwise destroy it along with itself.
        gtk_window_set_child(GTK_WINDOW(mFloatWindow), nullptr);
        gtk_window_destroy(GTK_WINDOW(mFloatWindow));
        mFloatWindow = nullptr;
    }
    g_object_unref(mWrapper); // drop the reference taken in the constructor
}

void DetachablePanel::setContent(GtkWidget* content) {
    if (mContent) {
        gtk_box_remove(GTK_BOX(mWrapper), mContent);
    }
    mContent = content;
    if (mContent) {
        gtk_widget_set_vexpand(mContent, TRUE);
        gtk_box_append(GTK_BOX(mWrapper), mContent);
    }
}

void DetachablePanel::enableDragSource() {
    GtkDragSource* dragSource = gtk_drag_source_new();
    gtk_drag_source_set_actions(dragSource, GDK_ACTION_MOVE);
    g_signal_connect(dragSource, "prepare", G_CALLBACK(onDragPrepare), this);
    g_signal_connect(dragSource, "drag-begin", G_CALLBACK(onDragBegin), this);
    // Capture phase: claims the press on its way down to mHeader, ahead of
    // the enclosing GtkPaned's own (bubble-phase) resize-handle gesture, so
    // a press that lands on the header is never ambiguous with a nearby handle.
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(dragSource), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(mHeader, GTK_EVENT_CONTROLLER(dragSource));
}

GdkContentProvider* DetachablePanel::onDragPrepare(GtkDragSource*, double, double, gpointer) {
    return gdk_content_provider_new_typed(G_TYPE_STRING, kDockDragMimeContent);
}

void DetachablePanel::onDragBegin(GtkDragSource*, GdkDrag* drag, gpointer userData) {
    // Same-process only - the drop side reads this back via
    // gdk_drop_get_drag() to identify which panel is being dragged.
    g_object_set_data(G_OBJECT(drag), "regui-dock-panel-id", userData);
}

void DetachablePanel::setNextFloatGeometry(int x, int y, int width, int height) {
    mFloatX = x;
    mFloatY = y;
    if (width > 0) {
        mFloatW = width;
    }
    if (height > 0) {
        mFloatH = height;
    }
}

void DetachablePanel::captureFloatGeometry() const {
#ifdef GDK_WINDOWING_WIN32
    if (!mFloatWindow) {
        return;
    }
    HWND hwnd = hwndFor(GTK_WINDOW(mFloatWindow));
    if (!hwnd) {
        return;
    }
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    mFloatX = rect.left;
    mFloatY = rect.top;
    mFloatW = rect.right - rect.left;
    mFloatH = rect.bottom - rect.top;
#endif
}

void DetachablePanel::currentFloatGeometry(int& x, int& y, int& width, int& height) const {
    captureFloatGeometry(); // no-op (and harmless) if not currently floating
    x = mFloatX;
    y = mFloatY;
    width = mFloatW;
    height = mFloatH;
}

void DetachablePanel::setFloating(bool floating) {
    if (floating == mFloating) {
        return;
    }
    mFloating = floating;
    // The floating window has its own native titlebar (title + close button,
    // which redocks via onFloatCloseRequest) - showing our in-app header too
    // would just duplicate it. Docked, there's no native titlebar at all, so
    // this is the only way to see the title or grab it to redock elsewhere.
    gtk_widget_set_visible(mHeader, !mFloating);

    if (mFloating) {
        mFloatWindow = gtk_window_new();
        gtk_window_set_title(GTK_WINDOW(mFloatWindow), mTitle.c_str());
        gtk_window_set_transient_for(GTK_WINDOW(mFloatWindow), mMainWindow);
        gtk_window_set_modal(GTK_WINDOW(mFloatWindow), FALSE); // main window stays fully interactive
        gtk_window_set_destroy_with_parent(GTK_WINDOW(mFloatWindow), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(mFloatWindow), mFloatW, mFloatH);
        gtk_window_set_child(GTK_WINDOW(mFloatWindow), mWrapper);
        g_signal_connect(mFloatWindow, "close-request", G_CALLBACK(onFloatCloseRequest), this);
        gtk_window_present(GTK_WINDOW(mFloatWindow));

#ifdef GDK_WINDOWING_WIN32
        if (mFloatX >= 0 && mFloatY >= 0) {
            if (HWND hwnd = hwndFor(GTK_WINDOW(mFloatWindow))) {
                SetWindowPos(hwnd, nullptr, mFloatX, mFloatY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
#endif
    } else {
        captureFloatGeometry(); // remember where it was, for next time
        if (mFloatWindow) {
            gtk_window_set_child(GTK_WINDOW(mFloatWindow), nullptr); // unparents mWrapper
            gtk_window_destroy(GTK_WINDOW(mFloatWindow));
            mFloatWindow = nullptr;
        }
    }
}

gboolean DetachablePanel::onFloatCloseRequest(GtkWindow*, gpointer userData) {
    auto* self = static_cast<DetachablePanel*>(userData);
    if (self->mOnFloatCloseRequested) {
        self->mOnFloatCloseRequested();
    }
    return TRUE; // the callback (via MainWindow::setPanelEdge) already handles tearing the window down
}

} // namespace regui::ui
