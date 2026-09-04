#include "ui/history_window.h"

#include <utility>

namespace regui::ui {

namespace {
// Freed via g_object_set_data_full when its button is destroyed (each rebuild()
// tears down and recreates every row, same lifetime as the button itself).
struct JumpContext {
    HistoryWindow* self;
    std::function<void(bool, size_t)>* onJump; // points at HistoryWindow::mOnJump - outlives every row
    bool isRedoTab;
    size_t displayIndex;
};
} // namespace

HistoryWindow::HistoryWindow(GtkWindow* parent, model::UndoStack* undoStack, std::function<void(bool, size_t)> onJump)
    : mUndoStack(undoStack), mOnJump(std::move(onJump)) {
    mWindow = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(mWindow), "Action History");
    gtk_window_set_default_size(GTK_WINDOW(mWindow), 480, 520);
    gtk_window_set_transient_for(GTK_WINDOW(mWindow), parent);
    gtk_window_set_modal(GTK_WINDOW(mWindow), FALSE); // the main window stays fully usable while this is open
    g_signal_connect(mWindow, "close-request", G_CALLBACK(onCloseRequest), this);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(mWindow), root);

    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);

    GtkWidget* switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_FILL);
    gtk_widget_set_margin_start(switcher, 8);
    gtk_widget_set_margin_end(switcher, 8);
    gtk_widget_set_margin_top(switcher, 8);
    gtk_widget_set_margin_bottom(switcher, 8);
    gtk_box_append(GTK_BOX(root), switcher);
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root), stack);
    gtk_widget_set_vexpand(stack, TRUE);

    auto buildPage = [](GtkWidget** outBox) -> GtkWidget* {
        GtkWidget* scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start(box, 12);
        gtk_widget_set_margin_end(box, 12);
        gtk_widget_set_margin_top(box, 12);
        gtk_widget_set_margin_bottom(box, 12);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
        *outBox = box;
        return scrolled;
    };

    gtk_stack_add_titled(GTK_STACK(stack), buildPage(&mUndoBox), "undo", "Undo");
    gtk_stack_add_titled(GTK_STACK(stack), buildPage(&mRedoBox), "redo", "Redo");

    undoStack->addOnChanged([this]() { rebuild(); });
    rebuild();
}

void HistoryWindow::present() { gtk_window_present(GTK_WINDOW(mWindow)); }

void HistoryWindow::rebuild() {
    for (GtkWidget* box : {mUndoBox, mRedoBox}) {
        GtkWidget* child = gtk_widget_get_first_child(box);
        while (child) {
            GtkWidget* next = gtk_widget_get_next_sibling(child);
            gtk_box_remove(GTK_BOX(box), child);
            child = next;
        }
    }
    populateList(mUndoBox, mUndoStack->undoEntries(), false);
    populateList(mRedoBox, mUndoStack->redoEntries(), true);
}

void HistoryWindow::populateList(GtkWidget* box, const std::vector<model::UndoStack::Entry>& entries, bool isRedoTab) {
    if (entries.empty()) {
        GtkWidget* empty = gtk_label_new("Nothing here yet");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_label_set_xalign(GTK_LABEL(empty), 0.0f);
        gtk_box_append(GTK_BOX(box), empty);
        return;
    }

    // undoEntries()/redoEntries() are oldest-first (stack order) - list most-recent-first instead.
    size_t displayIndex = 0;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it, ++displayIndex) {
        GtkWidget* label;
        if (it->detail.empty()) {
            label = gtk_label_new(it->description.c_str());
            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        } else {
            label = gtk_expander_new(it->description.c_str());
            gtk_expander_set_expanded(GTK_EXPANDER(label), displayIndex == 0); // the most recent action starts open

            GtkWidget* detailLabel = gtk_label_new(it->detail.c_str());
            gtk_label_set_xalign(GTK_LABEL(detailLabel), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(detailLabel), TRUE);
            gtk_widget_add_css_class(detailLabel, "dim-label");
            gtk_widget_set_margin_start(detailLabel, 12);
            gtk_widget_set_margin_top(detailLabel, 4);
            gtk_expander_set_child(GTK_EXPANDER(label), detailLabel);
        }
        gtk_widget_set_hexpand(label, TRUE);

        GtkWidget* jumpButton = gtk_button_new_with_label("Jump Here");
        gtk_widget_add_css_class(jumpButton, "flat");
        gtk_widget_set_tooltip_text(
            jumpButton, isRedoTab ? "Redo everything up to and including this action"
                                  : "Undo everything after this action");
        auto* ctx = new JumpContext{this, &mOnJump, isRedoTab, displayIndex};
        g_object_set_data_full(G_OBJECT(jumpButton), "regui-jump-ctx", ctx, [](gpointer p) { delete static_cast<JumpContext*>(p); });
        g_signal_connect(jumpButton, "clicked", G_CALLBACK(onJumpButtonClicked), ctx);

        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_append(GTK_BOX(row), label);
        gtk_box_append(GTK_BOX(row), jumpButton);
        gtk_box_append(GTK_BOX(box), row);
    }
}

gboolean HistoryWindow::onCloseRequest(GtkWindow* window, gpointer) {
    gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
    return TRUE; // hide, don't destroy - this window's UndoStack subscription lives for the app's lifetime
}

void HistoryWindow::onJumpButtonClicked(GtkButton*, gpointer userData) {
    auto* ctx = static_cast<JumpContext*>(userData);
    if (*ctx->onJump) {
        (*ctx->onJump)(ctx->isRedoTab, ctx->displayIndex);
    }
}

} // namespace regui::ui
