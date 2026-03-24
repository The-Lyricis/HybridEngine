#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace Hybrid
{
    struct EditorContext;

    class IEditorCommand
    {
    public:
        virtual ~IEditorCommand() = default;

        virtual void undo(EditorContext& ctx) = 0;
        virtual void redo(EditorContext& ctx) = 0;
        virtual const char* name() const = 0;
    };

    class CommandHistory
    {
    public:
        bool canUndo() const;
        bool canRedo() const;

        const char* peekUndoName() const;
        const char* peekRedoName() const;

        void execute(std::unique_ptr<IEditorCommand> command, EditorContext& ctx);
        bool undo(EditorContext& ctx);
        bool redo(EditorContext& ctx);
        void clear();

    private:
        std::vector<std::unique_ptr<IEditorCommand>> m_undo_stack;
        std::vector<std::unique_ptr<IEditorCommand>> m_redo_stack;
    };
} // namespace Hybrid
