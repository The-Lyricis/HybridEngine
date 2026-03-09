#include "inspector_panel.h"
#include "editor/core/component_registry.h"
#include "editor/core/editor_context.h"
#include "editor/core/property_drawer.h"

#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/entity.h"

#include <imgui.h>
#include <entt/entt.hpp>

namespace Hybrid
{
    void InspectorPanel::onImGuiRender(EditorContext& ctx)
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
        Entity selected_entity(ctx.selected, &reg, ctx.active_scene);

        if (ctx.selected == entt::null || !reg.valid(ctx.selected))
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        for (const ComponentDesc& desc : ComponentRegistry::GetDescriptors())
        {
            if (!desc.has || !desc.has(selected_entity))
                continue;

            void* component_ptr = desc.get ? desc.get(selected_entity) : nullptr;
            if (component_ptr == nullptr)
                continue;

            if (!ImGui::CollapsingHeader(desc.name, ImGuiTreeNodeFlags_DefaultOpen))
                continue;

            ImGui::PushID(desc.name);

            if (desc.draw_custom)
            {
                if (desc.draw_custom(ctx, selected_entity, component_ptr))
                    ctx.markSceneDirty();
            }
            else
            {
                for (const PropertyDesc& property : desc.properties)
                {
                    if (DrawPropertyField(component_ptr, property))
                        ctx.markSceneDirty();
                }
            }

            ImGui::PopID();
        }

        ImGui::End();
    }
}
