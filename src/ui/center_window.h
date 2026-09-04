#pragma once

#include <gtk/gtk.h>

namespace regui::ui {

// Centers `window` on its monitor - GTK4 has no portable API for this, so
// it's a Win32-specific workaround (no-op elsewhere). Call after present()/show().
void centerWindowOnScreen(GtkWindow* window);

} // namespace regui::ui
