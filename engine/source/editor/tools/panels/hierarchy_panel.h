#pragma once
#include "i_editor_panel.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <vector>
#include <unordered_set>

namespace Hybrid
{
    struct EditorContext;

    class HierarchyPanel final : public IEditorPanel
    {
    public:
        HierarchyPanel() : IEditorPanel(EditorPanelId::Hierarchy, "Hierarchy") {}
        void onImGuiRender(EditorContext& ctx) override;

    private:
        enum class PendingActionType : uint8_t
        {
            None = 0,
            Delete,
            Unparent,
            CreateRootEmpty,
            CreateRootCube,
            CreateRootCamera,
            CreateRootDirectionalLight,
            CreateRootPointLight,
        };

        void drawWindowContextMenu(EditorContext& ctx) override;
        void drawEntityNode(EditorContext& ctx,
                            entt::registry& registry,
                            entt::entity entity,
                            std::unordered_set<uint32_t>& visited);
        void drawEntityContextMenu(EditorContext& ctx, entt::registry& registry, entt::entity entity);
        void drawRootDropTarget(EditorContext& ctx, entt::registry& registry);
        void queueAction(PendingActionType type, entt::entity target = entt::null);
        void flushPendingAction(EditorContext& ctx, entt::registry& registry);
        void drawCommonContextMenu(EditorContext& ctx, entt::registry& registry, entt::entity target);
        void collectEntityOrder(entt::registry& registry, entt::entity entity, std::vector<entt::entity>& order) const;
        void applySelectionClick(EditorContext& ctx, entt::entity entity);

    private:
        PendingActionType m_pendingAction = PendingActionType::None;
        entt::entity m_pendingTarget = entt::null;
        std::vector<entt::entity> m_visibleOrder;
    };
}
