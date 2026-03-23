#include "inspector_panel.h"
#include "editor/core/component_registry.h"
#include "editor/core/editor_context.h"
#include "editor/core/property_drawer.h"

#include "runtime/core/base/macro.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/components/collider_component.h"

#include <glad/gl.h>
#include <imgui.h>
#include <entt/entt.hpp>
#include <stb_image.h>
#include <cstdint>
#include <cstring>
#include <string>

namespace Hybrid
{
    namespace
    {
        constexpr const char* kInspectorPanelLogTag = "[InspectorPanel]";

        uint32_t entityHandleValue(entt::entity entity)
        {
            return entt::to_integral(entity);
        }

        static GLuint LoadTextureRGBA8(const std::string& path)
        {
            int w = 0, h = 0, comp = 0;
            stbi_set_flip_vertically_on_load(0);
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
            if (!data || w <= 0 || h <= 0)
            {
                if (data)
                    stbi_image_free(data);
                return 0;
            }

            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);

            stbi_image_free(data);
            return tex;
        }

        static GLuint GetTrashIconTexture()
        {
            static GLuint s_trash_icon = 0;
            static bool s_loaded = false;
            if (!s_loaded)
            {
                s_loaded = true;
                const std::string base = std::string(HYBRID_ROOT_DIR) + "/resources/icons/";
                s_trash_icon = LoadTextureRGBA8(base + "icon_component_trash.png");
            }
            return s_trash_icon;
        }

        struct ComponentHeaderResult
        {
            bool open = false;
            bool enabled_changed = false;
            bool remove_clicked = false;
        };

        static ComponentHeaderResult DrawComponentHeader(const ComponentDesc& desc,
                                                         void* component_ptr,
                                                         bool can_remove)
        {
            ComponentHeaderResult result{};

            const ImGuiTreeNodeFlags header_flags =
                ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_Framed |
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_AllowOverlap;

            ImGui::SetNextItemAllowOverlap();
            result.open = ImGui::CollapsingHeader("##ComponentHeader", header_flags);

            const ImVec2 rect_min = ImGui::GetItemRectMin();
            const ImVec2 rect_max = ImGui::GetItemRectMax();
            const ImVec2 rect_size(rect_max.x - rect_min.x, rect_max.y - rect_min.y);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            const ImGuiStyle& style = ImGui::GetStyle();
            const float font_size = ImGui::GetFontSize();
            const float checkbox_size = ImGui::GetFrameHeight() - 4.0f;
            const float icon_button_width = 24.0f;
            const float icon_size = 14.0f;

            float text_x = rect_min.x + style.FramePadding.x + font_size + style.ItemInnerSpacing.x;

            if (desc.enabled != nullptr)
            {
                bool* enabled_ptr = desc.enabled(component_ptr);
                if (enabled_ptr != nullptr)
                {
                    const ImVec2 checkbox_pos(
                        text_x,
                        rect_min.y + (rect_size.y - checkbox_size) * 0.5f);
                    ImGui::SetCursorScreenPos(checkbox_pos);
                    ImGui::SetNextItemAllowOverlap();
                    if (ImGui::Checkbox("##Enabled", enabled_ptr))
                        result.enabled_changed = true;
                    text_x = checkbox_pos.x + checkbox_size + style.ItemInnerSpacing.x;
                }
            }

            const ImVec2 text_pos(
                text_x,
                rect_min.y + (rect_size.y - ImGui::GetTextLineHeight()) * 0.5f);
            draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), desc.name);

            if (can_remove)
            {
                const ImVec2 remove_button_size(icon_button_width, rect_size.y - 4.0f);
                const ImVec2 remove_button_pos(
                    rect_max.x - style.FramePadding.x - remove_button_size.x,
                    rect_min.y + (rect_size.y - remove_button_size.y) * 0.5f);

                ImGui::SetCursorScreenPos(remove_button_pos);
                ImGui::SetNextItemAllowOverlap();
                if (ImGui::Button("##RemoveComponent", remove_button_size))
                    result.remove_clicked = true;

                if (const GLuint trash_icon = GetTrashIconTexture(); trash_icon != 0)
                {
                    const ImVec2 icon_pos(
                        remove_button_pos.x + (remove_button_size.x - icon_size) * 0.5f,
                        remove_button_pos.y + (remove_button_size.y - icon_size) * 0.5f);
                    draw_list->AddImage(
                        (ImTextureID)(intptr_t)trash_icon,
                        icon_pos,
                        ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size));
                }
            }

            return result;
        }
    }

    void InspectorPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open) return;

        ImGui::Begin(getName(), &m_state.open);

        if (!ctx.active_scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        auto& reg = ctx.active_scene->getRegistry();
        Entity selected_entity(ctx.activeEntity(), &reg, ctx.active_scene);

        if (ctx.activeEntity() == entt::null || !reg.valid(ctx.activeEntity()))
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        if (ctx.selection.size() > 1)
        {
            ImGui::TextDisabled("%zu objects selected. Showing active object.", ctx.selection.size());
            ImGui::Separator();
        }

        const ComponentDesc* pending_remove_desc = nullptr;

        for (const ComponentDesc& desc : ComponentRegistry::GetDescriptors())
        {
            if (!desc.has || !desc.has(selected_entity))
                continue;

            void* component_ptr = desc.get ? desc.get(selected_entity) : nullptr;
            if (component_ptr == nullptr)
                continue;

            ImGui::PushID(desc.name);

            const bool is_play_mode = ctx.is_play_mode && ctx.is_play_mode();
            const bool can_remove =
                !is_play_mode &&
                HasAny(desc.flags, ComponentFlags::Removable) &&
                desc.remove != nullptr;

            const ComponentHeaderResult header = DrawComponentHeader(desc, component_ptr, can_remove);
            if (header.enabled_changed)
                ctx.markSceneDirty();
            if (header.remove_clicked)
                pending_remove_desc = &desc;

            if (header.open && pending_remove_desc == nullptr)
            {
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
            }

            ImGui::PopID();

            if (pending_remove_desc != nullptr)
                break;
        }

        if (pending_remove_desc != nullptr && pending_remove_desc->remove != nullptr)
        {
            HBD_CORE_INFO("{} component_remove_requested entity={} component={}",
                          kInspectorPanelLogTag,
                          entityHandleValue(selected_entity.GetHandle()),
                          pending_remove_desc->name);
            pending_remove_desc->remove(selected_entity);
            ctx.markSceneDirty();
            HBD_CORE_INFO("{} component_remove_completed entity={} component={}",
                          kInspectorPanelLogTag,
                          entityHandleValue(selected_entity.GetHandle()),
                          pending_remove_desc->name);
        }

        ImGui::Separator();

        const bool is_play_mode = ctx.is_play_mode && ctx.is_play_mode();
        if (is_play_mode)
            ImGui::BeginDisabled();

        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("##InspectorAddComponent");

        if (is_play_mode)
            ImGui::EndDisabled();

        if (ImGui::BeginPopup("##InspectorAddComponent"))
        {
            bool has_addable_entry = false;

            for (const ComponentDesc& desc : ComponentRegistry::GetDescriptors())
            {
                if (!HasAny(desc.flags, ComponentFlags::Addable) || desc.add == nullptr)
                    continue;

                has_addable_entry = true;

                const bool already_has_component = desc.has && desc.has(selected_entity);
                if (already_has_component)
                    ImGui::BeginDisabled();

                if (ImGui::MenuItem(desc.name))
                {
                    if (!already_has_component && desc.add(selected_entity))
                    {
                        HBD_CORE_INFO("{} component_add_completed entity={} component={}",
                                      kInspectorPanelLogTag,
                                      entityHandleValue(selected_entity.GetHandle()),
                                      desc.name);
                        if (std::strcmp(desc.name, "BoxCollider") == 0 && ctx.fit_box_collider_to_mesh)
                            (void)ctx.fit_box_collider_to_mesh(selected_entity.GetHandle());
                        ctx.markSceneDirty();
                    }
                    else if (already_has_component)
                    {
                        HBD_CORE_WARN("{} component_add_rejected entity={} component={} reason=already_exists",
                                      kInspectorPanelLogTag,
                                      entityHandleValue(selected_entity.GetHandle()),
                                      desc.name);
                    }
                    else
                    {
                        HBD_CORE_WARN("{} component_add_failed entity={} component={}",
                                      kInspectorPanelLogTag,
                                      entityHandleValue(selected_entity.GetHandle()),
                                      desc.name);
                    }
                    ImGui::CloseCurrentPopup();
                }

                if (already_has_component)
                    ImGui::EndDisabled();
            }

            if (!has_addable_entry)
                ImGui::TextDisabled("No addable components.");

            ImGui::EndPopup();
        }

        ImGui::End();
    }
}
