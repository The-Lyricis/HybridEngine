#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include <entt/entt.hpp>
#include <imgui.h>

#include "runtime/function/asset/asset_type.h"

namespace Hybrid::EditorDragDrop
{
    inline constexpr const char* ENTITY = "HY_ENTITY";
    inline constexpr const char* ASSET = "HY_ASSET";
    inline constexpr const char* PROJECT_PATH = "HY_PROJ_PATH";

    struct EntityPayload
    {
        entt::entity handle{entt::null};
    };

    struct AssetPayload
    {
        AssetID id{};
    };

    struct PathPayload
    {
        char rel[260]{};
    };

    inline void BeginDragEntity(entt::entity entity, const char* label = "Entity")
    {
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            return;

        EntityPayload payload{entity};
        ImGui::SetDragDropPayload(ENTITY, &payload, sizeof(payload));
        ImGui::TextUnformatted(label);
        ImGui::EndDragDropSource();
    }

    inline bool AcceptEntity(entt::entity& outEntity)
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ENTITY))
        {
            if (payload->DataSize == sizeof(EntityPayload))
            {
                const auto* p = static_cast<const EntityPayload*>(payload->Data);
                outEntity = p->handle;
                return true;
            }
        }
        return false;
    }

    inline void BeginDragAsset(AssetID id)
    {
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            return;

        AssetPayload payload{id};
        ImGui::SetDragDropPayload(ASSET, &payload, sizeof(payload));
        ImGui::Text("Asset: %llu", static_cast<unsigned long long>(id.value));
        ImGui::EndDragDropSource();
    }

    inline bool AcceptAsset(AssetID& outId)
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ASSET))
        {
            if (payload->DataSize == sizeof(AssetPayload))
            {
                const auto* p = static_cast<const AssetPayload*>(payload->Data);
                outId = p->id;
                return true;
            }
        }
        return false;
    }

    inline void BeginDragProjectPath(const char* relPath)
    {
        if (!relPath)
            return;
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            return;

        PathPayload payload{};
        std::snprintf(payload.rel, sizeof(payload.rel), "%s", relPath);
        ImGui::SetDragDropPayload(PROJECT_PATH, &payload, sizeof(payload));
        ImGui::TextUnformatted(relPath);
        ImGui::EndDragDropSource();
    }

    inline bool AcceptProjectPath(std::string& outRelPath)
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PROJECT_PATH))
        {
            if (payload->DataSize == sizeof(PathPayload))
            {
                const auto* p = static_cast<const PathPayload*>(payload->Data);
                outRelPath = p->rel;
                return true;
            }
        }
        return false;
    }
} // namespace Hybrid::EditorDragDrop
