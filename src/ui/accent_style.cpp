#include "ui/accent_style.h"

#include <string>

namespace regui::ui {

namespace {

int gNextAccentId = 0;

// Stamps `widget` with a fresh, never-reused class and returns its name (no leading dot), so
// callers can build a selector that will only ever match this one widget.
std::string addUniqueAccentClass(GtkWidget* widget) {
    std::string name = "regui-accent-" + std::to_string(gNextAccentId++);
    gtk_widget_add_css_class(widget, name.c_str());
    return name;
}

void loadGlobalCss(const std::string& css) {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css.c_str());
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

} // namespace

void applyAccentButtonStyle(GtkWidget* button) {
    std::string cls = addUniqueAccentClass(button);
    loadGlobalCss(
        // `background` (shorthand) clears the theme's own gradient/image in one declaration,
        // rather than fighting it property-by-property.
        "." + cls +
        " {"
        "  background: #ED8200 none;"
        "  border-color: #b56400;"
        "  box-shadow: none;"
        "  color: white;"
        "}"
        "." +
        cls + ":hover { background-color: #ff9012; }" + "." + cls +
        ":active, ." + cls + ":checked { background-color: #b56400; }" + "." + cls +
        ":disabled { background-color: rgba(237, 130, 0, 0.35); color: rgba(255, 255, 255, 0.6); }");
}

void applyAccentProgressBarStyle(GtkWidget* progressBar) {
    std::string cls = addUniqueAccentClass(progressBar);
    loadGlobalCss("." + cls + " trough progress { background-color: #ED8200; }");
}

void applyAccentCheckButtonStyle(GtkWidget* checkButton) {
    std::string cls = addUniqueAccentClass(checkButton);
    loadGlobalCss("." + cls + " check:checked { background-color: #ED8200; border-color: #ED8200; }");
}

void applyAccentFocusStyle(GtkWidget* widget) {
    std::string cls = addUniqueAccentClass(widget);
    loadGlobalCss("." + cls + ":focus { outline-color: #ED8200; }");
}

void applyAccentRowSelectionStyle(GtkWidget* listOrBox) {
    std::string cls = addUniqueAccentClass(listOrBox);
    loadGlobalCss(
        "." + cls + " row:selected { background-color: #ED8200; color: white; }" + "." + cls +
        " row:selected:hover { background-color: #ff9012; }");
}

} // namespace regui::ui
