#pragma once
#include "i_editor_panel.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <unordered_set>

namespace Hybrid
{
    class HierarchyPanel final : public IEditorPanel
    {
    public:
        const char* getName() const override { return "Hierarchy"; }
        void onImGuiRender(EditorContext& ctx) override;

    private:
        void drawEntityNode(EditorContext& ctx,
                            entt::registry& registry,
                            entt::entity entity,
                            std::unordered_set<uint32_t>& visited);
        void drawRootDropTarget(EditorContext& ctx, entt::registry& registry);
    };
}
