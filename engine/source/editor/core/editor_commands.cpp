#include "editor_commands.h"

#include "editor/core/editor_context.h"

namespace Hybrid
{
    namespace
    {
        bool isPlayMode(const EditorContext& ctx)
        {
            return ctx.is_play_mode && ctx.is_play_mode();
        }

        bool hasActiveDocumentScene(const EditorContext& ctx)
        {
            return ctx.active_document && ctx.active_document->scene != nullptr;
        }
    } // namespace

    bool EditorCommandDispatcher::canExecute(EditorCommandId id, const EditorCommandContext& ctx) const
    {
        const EditorContext* editor = ctx.editor;
        if (editor == nullptr)
            return false;

        const bool playing = isPlayMode(*editor);
        switch (id)
        {
        case EditorCommandId::NewScene:
            return !playing && static_cast<bool>(editor->request_new_scene);
        case EditorCommandId::OpenScene:
            return !playing && static_cast<bool>(editor->request_open_scene);
        case EditorCommandId::SaveScene:
            return !playing && static_cast<bool>(editor->request_save_scene) && hasActiveDocumentScene(*editor);
        case EditorCommandId::SaveSceneAs:
            return !playing && static_cast<bool>(editor->request_save_scene_as) && hasActiveDocumentScene(*editor);
        case EditorCommandId::EnterPlayMode:
            return !playing && static_cast<bool>(editor->enter_play_mode) && hasActiveDocumentScene(*editor);
        case EditorCommandId::ExitPlayMode:
            return playing && static_cast<bool>(editor->exit_play_mode);
        case EditorCommandId::TogglePauseMode:
            return playing && static_cast<bool>(editor->toggle_pause_mode);
        case EditorCommandId::ResetLayout:
            return static_cast<bool>(editor->request_reset_layout);
        }

        return false;
    }

    bool EditorCommandDispatcher::execute(EditorCommandId id, EditorCommandContext& ctx) const
    {
        EditorContext* editor = ctx.editor;
        if (editor == nullptr || !canExecute(id, ctx))
            return false;

        switch (id)
        {
        case EditorCommandId::NewScene:
            return editor->request_new_scene();
        case EditorCommandId::OpenScene:
            return editor->request_open_scene();
        case EditorCommandId::SaveScene:
            return editor->request_save_scene();
        case EditorCommandId::SaveSceneAs:
            return editor->request_save_scene_as();
        case EditorCommandId::EnterPlayMode:
            editor->enter_play_mode();
            return true;
        case EditorCommandId::ExitPlayMode:
            editor->exit_play_mode();
            return true;
        case EditorCommandId::TogglePauseMode:
            editor->toggle_pause_mode();
            return true;
        case EditorCommandId::ResetLayout:
            editor->request_reset_layout();
            return true;
        }

        return false;
    }
} // namespace Hybrid
