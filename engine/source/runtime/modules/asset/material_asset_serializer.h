#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "material.h"

namespace Hybrid
{
    constexpr uint32_t kMaterialAssetSchemaVersion = 1;

    struct MaterialAssetIssue
    {
        bool fatal = false;
        std::string field;
        std::string reason;
        std::string value;
    };

    struct MaterialAssetDesc
    {
        std::string name;
        MaterialData data{};

        std::string base_color_texture_path;
        std::string normal_texture_path;
        std::string metallic_roughness_texture_path;
        std::string occlusion_texture_path;
        std::string emissive_texture_path;

        std::string import_source;
        std::vector<std::string> import_notes;
    };

    struct MaterialAssetParseResult
    {
        MaterialAssetDesc desc{};
        std::vector<MaterialAssetIssue> issues;

        bool hasFatalIssue() const;
    };

    MaterialAssetParseResult ParseMaterialAssetJson(const nlohmann::json& root);
    nlohmann::json SerializeMaterialAssetJson(const MaterialAssetDesc& desc);
}
