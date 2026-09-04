#pragma once

#include <functional>
#include <string>
#include <vector>

namespace regui::model {

// Command-pattern undo/redo stack. Pushing clears the redo stack.
class UndoStack {
public:
    // `description` is a short one-line label (e.g. "Widget Moved"); `detail`
    // is an optional longer line shown underneath in the history view (e.g.
    // "Moved from {X=0,Y=0} to {X=10,Y=0}") - empty means no expandable row.
    struct Entry {
        std::function<void()> undo;
        std::function<void()> redo;
        std::string description;
        std::string detail;
    };

    void push(std::function<void()> undo, std::function<void()> redo, std::string description, std::string detail = {}) {
        mUndoStack.push_back({std::move(undo), std::move(redo), std::move(description), std::move(detail)});
        mRedoStack.clear();
        notifyChanged();
    }

    bool canUndo() const { return !mUndoStack.empty(); }
    bool canRedo() const { return !mRedoStack.empty(); }

    // Most-recent first (mUndoStack.back()/mRedoStack.back() is entry [0]) -
    // matches how a history view lists them, and how undo()/redo() consume them.
    const std::vector<Entry>& undoEntries() const { return mUndoStack; }
    const std::vector<Entry>& redoEntries() const { return mRedoStack; }

    void undo() {
        if (stepUndo()) {
            notifyChanged();
        }
    }

    void redo() {
        if (stepRedo()) {
            notifyChanged();
        }
    }

    // Undoes exactly `count` actions in one go (e.g. jumping to a History
    // window entry: `count` is that entry's most-recent-first display index,
    // so the entry itself stays applied and everything after it is undone).
    // Stops early (no-op past that point) if the stack runs out first.
    // Returns whether anything actually changed.
    bool undoTo(size_t count) {
        bool changed = false;
        for (size_t i = 0; i < count && stepUndo(); ++i) {
            changed = true;
        }
        if (changed) {
            notifyChanged();
        }
        return changed;
    }

    // Redoes exactly `count` actions in one go (e.g. jumping to a History
    // window Redo-tab entry: `count` is that entry's display index + 1, so
    // it and everything before it in the redo stack gets reapplied).
    bool redoTo(size_t count) {
        bool changed = false;
        for (size_t i = 0; i < count && stepRedo(); ++i) {
            changed = true;
        }
        if (changed) {
            notifyChanged();
        }
        return changed;
    }

    void clear() {
        mUndoStack.clear();
        mRedoStack.clear();
        notifyChanged();
    }

    // Fired after push()/undo()/redo()/clear() - e.g. a History window uses
    // this to live-refresh. Multiple subscribers are all kept (never overwritten).
    void addOnChanged(std::function<void()> callback) { mOnChanged.push_back(std::move(callback)); }

private:
    bool stepUndo() {
        if (mUndoStack.empty()) {
            return false;
        }
        Entry entry = std::move(mUndoStack.back());
        mUndoStack.pop_back();
        entry.undo();
        mRedoStack.push_back(std::move(entry));
        return true;
    }

    bool stepRedo() {
        if (mRedoStack.empty()) {
            return false;
        }
        Entry entry = std::move(mRedoStack.back());
        mRedoStack.pop_back();
        entry.redo();
        mUndoStack.push_back(std::move(entry));
        return true;
    }

    void notifyChanged() {
        for (auto& callback : mOnChanged) {
            callback();
        }
    }

    std::vector<Entry> mUndoStack;
    std::vector<Entry> mRedoStack;
    std::vector<std::function<void()>> mOnChanged;
};

} // namespace regui::model
