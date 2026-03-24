#include "property_drawer.h"

#include <cstring>
#include <string>

#include <glm/glm.hpp>
#include <imgui.h>

#include "editor/core/editor_drag_drop.h"

namespace Hybrid
{
    namespace
    {
        bool DrawAssetField(EditorContext* ctx,
                            Entity entity,
                            const PropertyDesc& property,
                            AssetID& asset_id)
        {
            std::string display = "None";
            if (const char* resolver_key = property.assetLabelResolverKey();
                resolver_key != nullptr && ctx != nullptr)
            {
                if (std::strcmp(resolver_key, "meshRendererMaterial") == 0 && ctx->asset_actions.describe_mesh_renderer_material)
                    display = ctx->asset_actions.describe_mesh_renderer_material(entity.GetHandle());
            }
            else if (ctx != nullptr && ctx->asset_actions.describe_asset)
            {
                display = ctx->asset_actions.describe_asset(asset_id);
            }
            else if (asset_id.value != 0)
            {
                display = std::to_string(asset_id.value);
            }

            bool changed = false;
            ImGui::PushID(property.name);
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 90.0f);
            ImGui::TextUnformatted(property.label());
            ImGui::NextColumn();

            char buffer[128]{};
            std::snprintf(buffer, sizeof(buffer), "%s", display.c_str());

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 64.0f);
            ImGui::InputText("##asset", buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);

            if (ImGui::BeginDragDropTarget())
            {
                AssetID dropped{};
                if (EditorDragDrop::AcceptAsset(dropped))
                {
                    const uint64_t asset_type_hint = property.assetTypeHint();
                    if (asset_type_hint == 0 || ctx == nullptr || !ctx->asset_actions.describe_asset)
                    {
                        asset_id = dropped;
                        changed = true;
                    }
                    else
                    {
                        asset_id = dropped;
                        changed = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Clear"))
            {
                if (asset_id.value != 0)
                {
                    asset_id = {};
                    changed = true;
                }
            }

            ImGui::Columns(1);
            ImGui::PopID();
            return changed;
        }
    }

    bool DrawPropertyField(EditorContext* ctx,
                           Entity entity,
                           void* componentPtr,
                           const PropertyDesc& property)
    {
        if (componentPtr == nullptr || !property.isVisible())
            return false;

        void* value_ptr = property.getMutablePtr(componentPtr);
        if (value_ptr == nullptr)
            return false;

        if (property.draw_override)
            return property.draw_override(property.label(), value_ptr);

        const bool editable = property.isEditable();

        if (!editable)
            ImGui::BeginDisabled();

        bool changed = false;
        ImGui::PushID(property.name);

        switch (property.resolvedValueKind())
        {
        case PropertyType::Bool:
            changed = ImGui::Checkbox(property.label(), static_cast<bool*>(value_ptr));
            break;
        case PropertyType::Int:
            if (property.hasRangeInfo())
                changed = ImGui::SliderInt(property.label(), static_cast<int*>(value_ptr),
                    static_cast<int>(property.rangeMinValue()), static_cast<int>(property.rangeMaxValue()));
            else
                changed = ImGui::DragInt(property.label(), static_cast<int*>(value_ptr),
                    property.stepValue() <= 0.0f ? 1.0f : property.stepValue());
            break;
        case PropertyType::Float:
            if (property.hasRangeInfo())
                changed = ImGui::SliderFloat(property.label(), static_cast<float*>(value_ptr),
                    property.rangeMinValue(), property.rangeMaxValue());
            else
                changed = ImGui::DragFloat(property.label(), static_cast<float*>(value_ptr),
                    property.stepValue());
            break;
        case PropertyType::String:
        {
            auto* value = static_cast<std::string*>(value_ptr);
            char buffer[256]{};
            strncpy_s(buffer, value->c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText(property.label(), buffer, sizeof(buffer)))
            {
                *value = buffer;
                changed = true;
            }
            break;
        }
        case PropertyType::Vec2:
            changed = ImGui::DragFloat2(property.label(), &static_cast<glm::vec2*>(value_ptr)->x,
                property.stepValue());
            break;
        case PropertyType::Vec3:
            if (property.isColorField())
                changed = ImGui::ColorEdit3(property.label(), &static_cast<glm::vec3*>(value_ptr)->x);
            else
                changed = ImGui::DragFloat3(property.label(), &static_cast<glm::vec3*>(value_ptr)->x,
                    property.stepValue());
            break;
        case PropertyType::Vec4:
            if (property.isColorField())
                changed = ImGui::ColorEdit4(property.label(), &static_cast<glm::vec4*>(value_ptr)->x);
            else
                changed = ImGui::DragFloat4(property.label(), &static_cast<glm::vec4*>(value_ptr)->x,
                    property.stepValue());
            break;
        case PropertyType::Enum:
            if (property.value_type != nullptr && property.value_type->isEnum())
            {
                int current = 0;
                switch (property.value_type->size)
                {
                case 1: current = static_cast<int>(*static_cast<uint8_t*>(value_ptr)); break;
                case 2: current = static_cast<int>(*static_cast<uint16_t*>(value_ptr)); break;
                case 4: current = static_cast<int>(*static_cast<uint32_t*>(value_ptr)); break;
                case 8: current = static_cast<int>(*static_cast<uint64_t*>(value_ptr)); break;
                default: current = 0; break;
                }

                changed = ImGui::Combo(property.label(),
                                       &current,
                                       property.value_type->enum_names,
                                       static_cast<int>(property.value_type->enum_name_count));
                if (changed)
                {
                    switch (property.value_type->size)
                    {
                    case 1: *static_cast<uint8_t*>(value_ptr) = static_cast<uint8_t>(current); break;
                    case 2: *static_cast<uint16_t*>(value_ptr) = static_cast<uint16_t>(current); break;
                    case 4: *static_cast<uint32_t*>(value_ptr) = static_cast<uint32_t>(current); break;
                    case 8: *static_cast<uint64_t*>(value_ptr) = static_cast<uint64_t>(current); break;
                    default: changed = false; break;
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("%s: enum metadata missing", property.label());
            }
            break;
        case PropertyType::Asset:
            changed = DrawAssetField(ctx, entity, property, *static_cast<AssetID*>(value_ptr));
            break;
        default:
            ImGui::TextDisabled("%s: unsupported property type", property.label());
            break;
        }

        if (const char* tooltip = property.tooltipText(); tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", tooltip);

        ImGui::PopID();

        if (!editable)
            ImGui::EndDisabled();

        return changed;
    }
} // namespace Hybrid
