#pragma once

#include <gtk/gtk.h>

namespace regui::ui {

// Every function here tints one specific widget with the app's own #ED8200 orange accent.
// GtkStyleContext's own per-widget provider API is deprecated since GTK 4.10 with no direct
// replacement, and empirically doesn't reliably restyle a widget's internal sub-nodes (a
// checkbutton's "check" indicator, a progress bar's "trough"/"progress") or pseudo-classes
// (":focus") even while it's still accepted - so instead, each function stamps `widget` with a
// fresh, never-reused CSS class (via the non-deprecated gtk_widget_add_css_class()) and loads a
// small rule for that exact class onto the display. The rule only ever matches the one widget
// that was given the class, even though it technically lives in the shared display-wide
// provider list. Call once, right after creating the widget.

// A "suggested-action" button.
void applyAccentButtonStyle(GtkWidget* button);

// A GtkProgressBar's fill.
void applyAccentProgressBarStyle(GtkWidget* progressBar);

// A GtkCheckButton's checked indicator.
void applyAccentCheckButtonStyle(GtkWidget* checkButton);

// Any widget's focus outline.
void applyAccentFocusStyle(GtkWidget* widget);

// A GtkListView/GtkListBox's row-selection highlight (the row itself is a wrapper widget the
// caller never gets a handle to, so this tags the list/box and matches its descendant rows).
void applyAccentRowSelectionStyle(GtkWidget* listOrBox);

} // namespace regui::ui
