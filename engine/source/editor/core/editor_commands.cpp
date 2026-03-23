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

        bool hasUnsavedSceneChanges(const EditorContext& ctx)
        {
            return ctx.active_document && ctx.active_document->dirty;
        }

        void queueUnsavedChangesConfirm(EditorContext& editor,
                                       std::string discard_label,
                                       std::function<bool()> on_save,
                                       std::function<void()> on_discard)
        {
            if (!editor.request_confirm_dialog)
            {
                if (on_discard)
                    on_discard();
                return;
            }

            EditorConfirmDialog dialog{};
            dialog.title = "Unsaved Changes";
            dialog.message = "The current scene has unsaved changes. Save before continuing?";
            dialog.confirm_label = "Save";
            dialog.secondary_label = std::move(discard_label);
            dialog.cancel_label = "Cancel";
            dialog.on_confirm = [on_save = std::move(on_save), on_discard = std::move(on_discard)]() mutable
            {
                if (on_save && on_save() && on_discard)
                    on_discard();
            };
            dialog.on_secondary = std::move(on_discard);
            editor.request_confirm_dialog(std::move(dialog));
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
            if (hasUnsavedSceneChanges(*editor))
            {
                queueUnsavedChangesConfirm(*editor,
                                           "Discard",
                                           [editor]() -> bool { return editor->request_save_scene(); },
                                           [editor]() {
                    editor->request_new_scene();
                });
                return true;
            }
            return editor->request_new_scene();
        case EditorCommandId::OpenScene:
            if (hasUnsavedSceneChanges(*editor))
            {
                queueUnsavedChangesConfirm(*editor,
                                           "Discard",
                                           [editor]() -> bool { return editor->request_save_scene(); },
                                           [editor]() {
                    editor->request_open_scene();
                });
                return true;
            }
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
