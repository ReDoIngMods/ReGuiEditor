#pragma once

#include <functional>
#include <vector>

namespace regui::model {

// Command-pattern undo/redo stack. Pushing clears the redo stack.
class UndoStack {
public:
    void push(std::function<void()> undo, std::function<void()> redo) {
        mUndoStack.push_back({std::move(undo), std::move(redo)});
        mRedoStack.clear();
    }

    bool canUndo() const { return !mUndoStack.empty(); }
    bool canRedo() const { return !mRedoStack.empty(); }

    void undo() {
        if (mUndoStack.empty()) {
            return;
        }
        Entry entry = std::move(mUndoStack.back());
        mUndoStack.pop_back();
        entry.undo();
        mRedoStack.push_back(std::move(entry));
    }

    void redo() {
        if (mRedoStack.empty()) {
            return;
        }
        Entry entry = std::move(mRedoStack.back());
        mRedoStack.pop_back();
        entry.redo();
        mUndoStack.push_back(std::move(entry));
    }

    void clear() {
        mUndoStack.clear();
        mRedoStack.clear();
    }

private:
    struct Entry {
        std::function<void()> undo;
        std::function<void()> redo;
    };

    std::vector<Entry> mUndoStack;
    std::vector<Entry> mRedoStack;
};

} // namespace regui::model
