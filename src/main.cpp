#include <gtk/gtk.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "canvas/mygui_canvas.h"
#include "ui/center_window.h"
#include "ui/loading_window.h"
#include "ui/main_window.h"
#include "ui/recent_files.h"
#include "ui/welcome_window.h"

namespace {

#ifdef _WIN32
// The exe is WIN32-subsystem (no console on a normal launch); opens one on
// demand and points stdout/stderr/stdin at it for -dev.
void attachDevConsole() {
    if (!AllocConsole()) {
        return;
    }
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);
}
#endif

void showWelcomeScreen(GtkApplication* app);

// Needs a display connection, which only exists once "activate"/"open" runs - called from both.
void ensureDarkTheme() { g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, nullptr); }

// Builds MainWindow, shows LoadingWindow while loadAssets() runs synchronously, then swaps it for the editor.
void openFile(GtkApplication* app, const std::string& path) {
    ensureDarkTheme();
    regui::ui::recent_files::addAndSave(path);

    // Registers itself as an app window immediately, so the app is never briefly down to zero windows.
    auto* mainWindow = new regui::ui::MainWindow(app, path);

    regui::ui::LoadingWindow loadingWindow(app);
    gtk_window_present(GTK_WINDOW(loadingWindow.window()));
    regui::ui::centerWindowOnScreen(GTK_WINDOW(loadingWindow.window()));
    // Pump once so the window paints before the blocking load below starts.
    while (g_main_context_pending(nullptr)) {
        g_main_context_iteration(nullptr, FALSE);
    }

    mainWindow->loadAssets(
        [&loadingWindow](const regui::canvas::LoadProgress& progress) { loadingWindow.update(progress); });

    gtk_window_destroy(GTK_WINDOW(loadingWindow.window()));
    gtk_window_present(GTK_WINDOW(mainWindow->window()));
    regui::ui::centerWindowOnScreen(GTK_WINDOW(mainWindow->window()));
    // Intentionally leaked for the app's lifetime - GTK owns the window.
}

void showWelcomeScreen(GtkApplication* app) {
    ensureDarkTheme();
    // Indirection cell: the callback needs to close the window, but the WelcomeWindow* doesn't exist yet.
    auto self = std::make_shared<regui::ui::WelcomeWindow*>(nullptr);
    *self = new regui::ui::WelcomeWindow(app, [app, self](std::string path) {
        gtk_window_destroy(GTK_WINDOW((*self)->window()));
        openFile(app, path);
    });
    gtk_window_present(GTK_WINDOW((*self)->window()));
    regui::ui::centerWindowOnScreen(GTK_WINDOW((*self)->window()));
}

void onActivate(GtkApplication* app, gpointer) { showWelcomeScreen(app); }

// Fired instead of "activate" when launched with a file argument; skips the welcome screen.
void onOpen(GtkApplication* app, GFile** files, gint nFiles, const gchar* /*hint*/, gpointer) {
    if (nFiles > 0) {
        if (char* path = g_file_get_path(files[0])) {
            openFile(app, path);
            g_free(path);
            return;
        }
    }
    showWelcomeScreen(app);
}

} // namespace

int main(int argc, char** argv) {
    // -dev is this app's own switch - stripped before g_application_run() so it isn't read as a file to open.
    bool devMode = false;
    std::vector<char*> filteredArgv;
    filteredArgv.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-dev") {
            devMode = true;
        } else {
            filteredArgv.push_back(argv[i]);
        }
    }

#ifdef _WIN32
    if (devMode) {
        attachDevConsole();
    }
#endif

    GtkApplication* app = gtk_application_new("com.redoingmods.reguieditor", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "activate", G_CALLBACK(onActivate), nullptr);
    g_signal_connect(app, "open", G_CALLBACK(onOpen), nullptr);
    int status = g_application_run(G_APPLICATION(app), static_cast<int>(filteredArgv.size()), filteredArgv.data());
    g_object_unref(app);
    return status;
}
