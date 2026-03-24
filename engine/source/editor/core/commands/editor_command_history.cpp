#include "editor_command_history.h"

#include "editor/core/context/editor_context.h"

namespace Hybrid
{
    bool CommandHistory::canUndo() const
    {
        return !m_undo_stack.empty();
    }

    bool CommandHistory::canRedo() const
    {
        return !m_redo_stack.empty();
    }

    const char* CommandHistory::peekUndoName() const
    {
        return canUndo() ? m_undo_stack.back()->name() : "";
    }

    const char* CommandHistory::peekRedoName() const
    {
        return canRedo() ? m_redo_stack.back()->name() : "";
    }

    void CommandHistory::execute(std::unique_ptr<IEditorCommand> command, EditorContext& ctx)
    {
        if (!command)
            return;

        command->redo(ctx);
        m_undo_stack.push_back(std::move(command));
        m_redo_stack.clear();
    }

    bool CommandHistory::undo(EditorContext& ctx)
    {
        if (!canUndo())
            return false;

        std::unique_ptr<IEditorCommand> command = std::move(m_undo_stack.back());
        m_undo_stack.pop_back();
        command->undo(ctx);
        m_redo_stack.push_back(std::move(command));
        return true;
    }

    bool CommandHistory::redo(EditorContext& ctx)
    {
        if (!canRedo())
            return false;

        std::unique_ptr<IEditorCommand> command = std::move(m_redo_stack.back());
        m_redo_stack.pop_back();
        command->redo(ctx);
        m_undo_stack.push_back(std::move(command));
        return true;
    }

    void CommandHistory::clear()
    {
        m_undo_stack.clear();
        m_redo_stack.clear();
    }
} // namespace Hybrid
