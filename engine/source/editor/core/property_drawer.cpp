#include "property_drawer.h"

#include <cstring>
#include <string>

#include <glm/glm.hpp>
#include <imgui.h>

namespace Hybrid
{
    bool DrawPropertyField(void* componentPtr, const PropertyDesc& property)
    {
        if (componentPtr == nullptr || !HasAny(property.flags, PropertyFlags::Visible))
            return false;

        void* value_ptr = static_cast<void*>(static_cast<char*>(componentPtr) + property.offset);

        if (property.draw_override)
            return property.draw_override(property.name, value_ptr);

        const bool editable =
            HasAny(property.flags, PropertyFlags::Editable) &&
            !HasAny(property.flags, PropertyFlags::ReadOnly);

        if (!editable)
            ImGui::BeginDisabled();

        bool changed = false;
        ImGui::PushID(property.name);

        switch (property.type)
        {
        case PropertyType::Bool:
            changed = ImGui::Checkbox(property.name, static_cast<bool*>(value_ptr));
            break;
        case PropertyType::Int:
            if (property.has_range)
                changed = ImGui::SliderInt(property.name, static_cast<int*>(value_ptr),
                    static_cast<int>(property.min), static_cast<int>(property.max));
            else
                changed = ImGui::DragInt(property.name, static_cast<int*>(value_ptr),
                    property.speed <= 0.0f ? 1.0f : property.speed);
            break;
        case PropertyType::Float:
            if (property.has_range)
                changed = ImGui::SliderFloat(property.name, static_cast<float*>(value_ptr),
                    property.min, property.max);
            else
                changed = ImGui::DragFloat(property.name, static_cast<float*>(value_ptr),
                    property.speed);
            break;
        case PropertyType::String:
        {
            auto* value = static_cast<std::string*>(value_ptr);
            char buffer[256]{};
            strncpy_s(buffer, value->c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText(property.name, buffer, sizeof(buffer)))
            {
                *value = buffer;
                changed = true;
            }
            break;
        }
        case PropertyType::Vec2:
            changed = ImGui::DragFloat2(property.name, &static_cast<glm::vec2*>(value_ptr)->x,
                property.speed);
            break;
        case PropertyType::Vec3:
            if (HasAny(property.flags, PropertyFlags::Color))
                changed = ImGui::ColorEdit3(property.name, &static_cast<glm::vec3*>(value_ptr)->x);
            else
                changed = ImGui::DragFloat3(property.name, &static_cast<glm::vec3*>(value_ptr)->x,
                    property.speed);
            break;
        case PropertyType::Vec4:
            if (HasAny(property.flags, PropertyFlags::Color))
                changed = ImGui::ColorEdit4(property.name, &static_cast<glm::vec4*>(value_ptr)->x);
            else
                changed = ImGui::DragFloat4(property.name, &static_cast<glm::vec4*>(value_ptr)->x,
                    property.speed);
            break;
        default:
            ImGui::TextDisabled("%s: unsupported property type", property.name);
            break;
        }

        if (property.tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", property.tooltip);

        ImGui::PopID();

        if (!editable)
            ImGui::EndDisabled();

        return changed;
    }
} // namespace Hybrid
