#pragma once

#include <memory>

#include <entt/entt.hpp>

#include "editor/core/commands/editor_command_history.h"
#include "editor/core/snapshot/entity_snapshot.h"
#include "editor/core/snapshot/selection_snapshot.h"

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

    class DuplicateEntityCommand final : public IEditorCommand
    {
    public:
        struct Entry
        {
            EntitySnapshot snapshot;
            UUID parent_id{};
        };

        DuplicateEntityCommand(std::shared_ptr<SceneDocument> document,
                               std::vector<Entry> entries,
                               EditorSelectionSnapshot before_selection,
                               EditorSelectionSnapshot after_selection);

        void undo(EditorContext& ctx) override;
        void redo(EditorContext& ctx) override;
        const char* name() const override;

    private:
        std::shared_ptr<SceneDocument> lockDocument() const;
        entt::entity resolveParent(Scene& scene, const UUID& parent_id) const;

    private:
        std::weak_ptr<SceneDocument> m_document;
        std::vector<Entry> m_entries;
        EditorSelectionSnapshot m_before_selection;
        EditorSelectionSnapshot m_after_selection;
    };

    class AddComponentCommand final : public IEditorCommand
    {
    public:
        AddComponentCommand(std::shared_ptr<SceneDocument> document,
                            UUID entity_id,
                            std::string component_name,
                            EntitySnapshot before_snapshot,
                            EntitySnapshot after_snapshot);

        void undo(EditorContext& ctx) override;
        void redo(EditorContext& ctx) override;
        const char* name() const override;

    private:
        bool apply(EditorContext& ctx, const EntitySnapshot& snapshot) const;
        std::shared_ptr<SceneDocument> lockDocument() const;

    private:
        std::weak_ptr<SceneDocument> m_document;
        UUID m_entity_id{};
        std::string m_name;
        EntitySnapshot m_before_snapshot;
        EntitySnapshot m_after_snapshot;
    };

    class RemoveComponentCommand final : public IEditorCommand
    {
    public:
        RemoveComponentCommand(std::shared_ptr<SceneDocument> document,
                               UUID entity_id,
                               std::string component_name,
                               EntitySnapshot before_snapshot,
                               EntitySnapshot after_snapshot);

        void undo(EditorContext& ctx) override;
        void redo(EditorContext& ctx) override;
        const char* name() const override;

    private:
        bool apply(EditorContext& ctx, const EntitySnapshot& snapshot) const;
        std::shared_ptr<SceneDocument> lockDocument() const;

    private:
        std::weak_ptr<SceneDocument> m_document;
        UUID m_entity_id{};
        std::string m_name;
        EntitySnapshot m_before_snapshot;
        EntitySnapshot m_after_snapshot;
    };
} // namespace Hybrid
