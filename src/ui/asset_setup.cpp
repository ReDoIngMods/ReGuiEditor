#include "ui/asset_setup.h"

#include "ui/game_assets.h"

#include <optional>
#include <utility>

namespace regui::ui::asset_setup {

namespace {

struct PromptContext {
    GtkApplication* app;
    std::function<void(std::string)> onReady;
};

void beginFolderPicker(PromptContext* ctx);

void onRetryChoice(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* ctx = static_cast<PromptContext*>(userData);
    GError* error = nullptr;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, &error);
    if (error) {
        g_error_free(error);
    }
    if (choice == 0) {
        beginFolderPicker(ctx);
    } else {
        g_application_quit(G_APPLICATION(ctx->app));
        delete ctx;
    }
}

void onFolderPickerReady(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* ctx = static_cast<PromptContext*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!file) {
        if (error) {
            g_error_free(error); // includes the user just cancelling the picker
        }
        g_application_quit(G_APPLICATION(ctx->app)); // nothing to fall back to
        delete ctx;
        return;
    }

    char* rawPath = g_file_get_path(file);
    std::string chosenRoot = rawPath ? rawPath : "";
    g_free(rawPath);
    g_object_unref(file);

    std::optional<std::string> guiPath = game_assets::guiPathFromGameRoot(chosenRoot);
    if (!guiPath) {
        GtkAlertDialog* alert = gtk_alert_dialog_new("That doesn't look like a Scrap Mechanic install");
        gtk_alert_dialog_set_detail(
            alert, "Couldn't find Data\\Gui\\GuiConfig.xml under that folder. Pick the game's install folder "
                   "again, or quit.");
        const char* buttons[] = {"Try Again", "Quit", nullptr};
        gtk_alert_dialog_set_buttons(alert, buttons);
        gtk_alert_dialog_set_cancel_button(alert, 1);
        gtk_alert_dialog_set_default_button(alert, 0);
        gtk_alert_dialog_choose(alert, nullptr, nullptr, onRetryChoice, ctx);
        g_object_unref(alert);
        return;
    }

    game_assets::storeGuiPath(*guiPath);
    ctx->onReady(*guiPath);
    delete ctx;
}

void beginFolderPicker(PromptContext* ctx) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Locate your Scrap Mechanic install folder");
    gtk_file_dialog_select_folder(dialog, nullptr, nullptr, onFolderPickerReady, ctx);
    g_object_unref(dialog);
}

void onExplainChoice(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* ctx = static_cast<PromptContext*>(userData);
    GError* error = nullptr;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, &error);
    if (error) {
        g_error_free(error);
    }
    if (choice == 0) {
        beginFolderPicker(ctx);
    } else {
        g_application_quit(G_APPLICATION(ctx->app));
        delete ctx;
    }
}

void promptForGameRoot(GtkApplication* app, std::function<void(std::string)> onReady) {
    auto* ctx = new PromptContext{app, std::move(onReady)};

    GtkAlertDialog* alert = gtk_alert_dialog_new("Locate Scrap Mechanic");
    gtk_alert_dialog_set_detail(
        alert, "ReGui Editor couldn't automatically find your Scrap Mechanic installation. On the next screen, "
               "pick the game's install folder (the one that contains a \"Data\" folder).");
    const char* buttons[] = {"Browse\xE2\x80\xA6", "Quit", nullptr};
    gtk_alert_dialog_set_buttons(alert, buttons);
    gtk_alert_dialog_set_cancel_button(alert, 1);
    gtk_alert_dialog_set_default_button(alert, 0);
    gtk_alert_dialog_choose(alert, nullptr, nullptr, onExplainChoice, ctx);
    g_object_unref(alert);
}

} // namespace

void resolveGuiAssetsPath(GtkApplication* app, std::function<void(std::string)> onReady) {
    if (std::optional<std::string> stored = game_assets::loadStoredGuiPath()) {
        onReady(*stored);
        return;
    }
    if (std::optional<std::string> found = game_assets::searchKnownLocations()) {
        game_assets::storeGuiPath(*found);
        onReady(*found);
        return;
    }
    promptForGameRoot(app, std::move(onReady));
}

} // namespace regui::ui::asset_setup
