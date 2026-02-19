#include "hierarchy_panel.h"
#include "../editor_context.h"

#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/components.h"

#include <imgui.h>
#include <entt/entt.hpp>

namespace Hybrid
{

    static const char* getEntityLabel(entt::registry& reg, entt::entity e)
    {
        if (auto* tag = reg.try_get<TagComponent>(e))
            return tag->Tag.c_str();
        return "Entity";
    }

    void HierarchyPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::Begin(getName(), &m_open);

        if (!ctx.active_scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        auto& reg = ctx.active_scene->getRegistry();

        // 点击空白处取消选择
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
            ctx.selected = entt::null;

        auto view = reg.view<TagComponent>();
        for (auto e : view)
        {
            const char* label = getEntityLabel(reg, e);
            const bool selected = (ctx.selected == e);

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_Leaf |
                (selected ? ImGuiTreeNodeFlags_Selected : 0);

            // 用 entt::to_integral(e) 作为 ImGui ID，避免 Entity 强转问题
            const auto id = (void*)(uintptr_t)entt::to_integral(e);
            bool opened = ImGui::TreeNodeEx(id, flags, "%s", label);

            if (ImGui::IsItemClicked())
                ctx.selected = e;

            if (opened)
                ImGui::TreePop();
        }

        ImGui::End();
    }
}
