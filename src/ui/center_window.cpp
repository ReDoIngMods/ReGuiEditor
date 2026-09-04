#include "ui/center_window.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WIN32
#include <gdk/win32/gdkwin32.h>
#endif

namespace regui::ui {

void centerWindowOnScreen(GtkWindow* window) {
#ifdef GDK_WINDOWING_WIN32
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (!surface || !GDK_IS_WIN32_SURFACE(surface)) {
        return;
    }
    HWND hwnd = gdk_win32_surface_get_handle(surface);

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return;
    }
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    // The actual monitor's work area, so this handles multi-monitor setups and excludes the taskbar.
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return;
    }
    const RECT& work = monitorInfo.rcWork;
    int x = work.left + ((work.right - work.left) - width) / 2;
    int y = work.top + ((work.bottom - work.top) - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
    (void)window;
#endif
}

} // namespace regui::ui
