#pragma once

namespace Hybrid
{
    struct EditorContext;

    enum class EditorCommandId
    {
        NewScene,
        OpenProject,
        OpenScene,
        SaveScene,
        SaveSceneAs,
        EnterPlayMode,
        ExitPlayMode,
        TogglePauseMode,
        ResetLayout,
    };

    struct EditorCommandContext
    {
        EditorContext* editor = nullptr;
    };

    class EditorCommandDispatcher
    {
    public:
        bool canExecute(EditorCommandId id, const EditorCommandContext& ctx) const;
        bool execute(EditorCommandId id, EditorCommandContext& ctx) const;
    };
} // namespace Hybrid
