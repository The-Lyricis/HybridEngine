#pragma once

#include <memory>

#include <entt/entt.hpp>

#include "editor/core/editor_command_history.h"
#include "editor/core/entity_snapshot.h"
#include "editor/core/selection_snapshot.h"

namespace Hybrid
{
    struct SceneDocument;

    class CreateEntityCommand final : public IEditorCommand
    {
    public:
        CreateEntityCommand(std::shared_ptr<SceneDocument> document,
                            EntitySnapshot snapshot,
                            entt::entity parent,
                            EditorSelectionSnapshot before_selection);

        void undo(EditorContext& ctx) override;
        void redo(EditorContext& ctx) override;
        const char* name() const override;

    private:
        std::shared_ptr<SceneDocument> lockDocument() const;
        entt::entity resolveParent(Scene& scene) const;

    private:
        std::weak_ptr<SceneDocument> m_document;
        EntitySnapshot m_snapshot;
        UUID m_parent_id{};
        EditorSelectionSnapshot m_before_selection;
    };

    class DeleteEntityCommand final : public IEditorCommand
    {
    public:
        DeleteEntityCommand(std::shared_ptr<SceneDocument> document,
                            EntitySnapshot snapshot,
                            entt::entity parent,
                            EditorSelectionSnapshot before_selection,
                            EditorSelectionSnapshot after_selection);

        void undo(EditorContext& ctx) override;
        void redo(EditorContext& ctx) override;
        const char* name() const override;

    private:
        std::shared_ptr<SceneDocument> lockDocument() const;
        entt::entity resolveParent(Scene& scene) const;

    private:
        std::weak_ptr<SceneDocument> m_document;
        EntitySnapshot m_snapshot;
        UUID m_parent_id{};
        EditorSelectionSnapshot m_before_selection;
        EditorSelectionSnapshot m_after_selection;
    };
} // namespace Hybrid
