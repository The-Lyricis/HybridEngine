#pragma once

#include <memory>

#include <entt/entt.hpp>

#include "editor/core/editor_command_history.h"
#include "editor/core/transform_snapshot.h"

namespace Hybrid
{
    struct SceneDocument;

    class TransformCommand final : public IEditorCommand
    {
    public:
        TransformCommand(std::shared_ptr<SceneDocument> document,
                         entt::entity entity,
                         TransformSnapshot before,
                         TransformSnapshot after);

        void undo(EditorContext& ctx) override;
        void redo(EditorContext& ctx) override;
        const char* name() const override;

    private:
        bool apply(EditorContext& ctx, const TransformSnapshot& snapshot) const;

    private:
        std::weak_ptr<SceneDocument> m_document;
        entt::entity m_entity{entt::null};
        TransformSnapshot m_before{};
        TransformSnapshot m_after{};
    };
} // namespace Hybrid
