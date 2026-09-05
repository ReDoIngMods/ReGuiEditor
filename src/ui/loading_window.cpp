#include "ui/loading_window.h"

#include "ui/accent_style.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace regui::ui {

LoadingWindow::LoadingWindow(GtkApplication* app) {
    mWindow = gtk_window_new();
    gtk_window_set_application(GTK_WINDOW(mWindow), app);
    gtk_window_set_title(GTK_WINDOW(mWindow), "ReGui Editor");
    gtk_window_set_default_size(GTK_WINDOW(mWindow), 440, 150);
    gtk_window_set_resizable(GTK_WINDOW(mWindow), FALSE);
    gtk_window_set_deletable(GTK_WINDOW(mWindow), FALSE); // nothing sensible to do if closed mid-load

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(root, 20);
    gtk_widget_set_margin_end(root, 20);
    gtk_widget_set_margin_top(root, 20);
    gtk_widget_set_margin_bottom(root, 20);
    gtk_widget_set_valign(root, GTK_ALIGN_CENTER);
    gtk_window_set_child(GTK_WINDOW(mWindow), root);

    mWhatLabel = gtk_label_new("Starting\xE2\x80\xA6");
    gtk_label_set_xalign(GTK_LABEL(mWhatLabel), 0.0f);
    gtk_widget_add_css_class(mWhatLabel, "heading");
    gtk_box_append(GTK_BOX(root), mWhatLabel);

    mWhereLabel = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(mWhereLabel), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(mWhereLabel), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_add_css_class(mWhereLabel, "dim-label");
    gtk_box_append(GTK_BOX(root), mWhereLabel);

    mWhoLabel = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(mWhoLabel), 0.0f);
    gtk_widget_add_css_class(mWhoLabel, "dim-label");
    gtk_box_append(GTK_BOX(root), mWhoLabel);

    mProgressBar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(mProgressBar), TRUE);
    gtk_widget_set_margin_top(mProgressBar, 8);
    applyAccentProgressBarStyle(mProgressBar);
    gtk_box_append(GTK_BOX(root), mProgressBar);

    update({"Starting\xE2\x80\xA6", "", "ReGui Editor", 0.0});
}

void LoadingWindow::update(const canvas::LoadProgress& progress) {
    double fraction = std::clamp(progress.fraction, 0.0, 1.0);

    gtk_label_set_text(GTK_LABEL(mWhatLabel), progress.what.c_str());
    gtk_label_set_text(GTK_LABEL(mWhereLabel), progress.where.empty() ? "" : ("File: " + progress.where).c_str());
    gtk_widget_set_visible(mWhereLabel, !progress.where.empty());
    gtk_label_set_text(GTK_LABEL(mWhoLabel), ("Component: " + progress.who).c_str());

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mProgressBar), fraction);
    std::string percentText = std::to_string(static_cast<int>(std::lround(fraction * 100))) + "%";
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mProgressBar), percentText.c_str());

    while (g_main_context_pending(nullptr)) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

} // namespace regui::ui
