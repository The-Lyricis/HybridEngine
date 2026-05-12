#include "material_asset_serializer.h"

#include <string>

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;

        void addIssue(std::vector<MaterialAssetIssue>& issues,
                      bool fatal,
                      std::string field,
                      std::string reason,
                      std::string value = {})
        {
            issues.push_back(MaterialAssetIssue{fatal, std::move(field), std::move(reason), std::move(value)});
        }

        std::string workflowToString(MaterialWorkflow workflow)
        {
            switch (workflow)
            {
            case MaterialWorkflow::MetallicRoughness:
            default:
                return "metallic_roughness";
            }
        }

        bool parseWorkflow(const json& node, MaterialWorkflow& out_workflow)
        {
            if (!node.is_string())
                return false;

            const std::string value = node.get<std::string>();
            if (value == "metallic_roughness" || value == "MetallicRoughness" ||
                value == "metallicRoughness" || value == "pbrMetallicRoughness")
            {
                out_workflow = MaterialWorkflow::MetallicRoughness;
                return true;
            }

            return false;
        }

        std::string alphaModeToString(MaterialAlphaMode mode)
        {
            switch (mode)
            {
            case MaterialAlphaMode::Mask:
                return "mask";
            case MaterialAlphaMode::Blend:
                return "blend";
            case MaterialAlphaMode::Opaque:
            default:
                return "opaque";
            }
        }

        bool parseAlphaMode(const json& node, MaterialAlphaMode& out_mode)
        {
            if (node.is_string())
            {
                const std::string value = node.get<std::string>();
                if (value == "opaque" || value == "OPAQUE")
                {
                    out_mode = MaterialAlphaMode::Opaque;
                    return true;
                }
                if (value == "mask" || value == "MASK" || value == "masked" || value == "alphatest" ||
                    value == "alpha_test")
                {
                    out_mode = MaterialAlphaMode::Mask;
                    return true;
                }
                if (value == "blend" || value == "BLEND" || value == "transparent" || value == "alphablend" ||
                    value == "alpha_blend")
                {
                    out_mode = MaterialAlphaMode::Blend;
                    return true;
                }
                return false;
            }

            if (node.is_number_integer() || node.is_number_unsigned())
            {
                const int value = node.get<int>();
                if (value < static_cast<int>(MaterialAlphaMode::Opaque) ||
                    value > static_cast<int>(MaterialAlphaMode::Blend))
                {
                    return false;
                }

                out_mode = static_cast<MaterialAlphaMode>(value);
                return true;
            }

            return false;
        }

        bool parseAssetId(const json& node, AssetID& out_id)
        {
            out_id = AssetID{};
            if (node.is_null())
                return true;

            if (node.is_number_unsigned())
            {
                out_id = AssetID::FromRaw(node.get<uint64_t>());
                return true;
            }
            if (node.is_number_integer())
            {
                const auto v = node.get<int64_t>();
                if (v < 0)
                    return false;
                out_id = AssetID::FromRaw(static_cast<uint64_t>(v));
                return true;
            }
            if (node.is_string())
            {
                const std::string s = node.get<std::string>();
                if (s.empty())
                    return true;
                try
                {
                    size_t idx = 0;
                    const uint64_t raw = std::stoull(s, &idx, 10);
                    if (idx != s.size())
                        return false;
                    out_id = AssetID::FromRaw(raw);
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            return false;
        }

        bool parseVec4(const json& node, glm::vec4& out_v)
        {
            if (!node.is_array() || node.size() != 4)
                return false;
            for (size_t i = 0; i < 4; ++i)
            {
                if (!node[i].is_number())
                    return false;
            }
            out_v = glm::vec4(node[0].get<float>(), node[1].get<float>(), node[2].get<float>(), node[3].get<float>());
            return true;
        }

        bool parseVec3(const json& node, glm::vec3& out_v)
        {
            if (!node.is_array() || node.size() != 3)
                return false;
            for (size_t i = 0; i < 3; ++i)
            {
                if (!node[i].is_number())
                    return false;
            }
            out_v = glm::vec3(node[0].get<float>(), node[1].get<float>(), node[2].get<float>());
            return true;
        }

        bool validateSchema(const json& root, std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains("version"))
            {
                addIssue(issues, true, "version", "missing_required_field");
                return false;
            }
            if (!root["version"].is_number_integer() && !root["version"].is_number_unsigned())
            {
                addIssue(issues, true, "version", "expected_integer");
                return false;
            }

            const uint32_t version = root["version"].get<uint32_t>();
            if (version != kMaterialAssetSchemaVersion)
            {
                addIssue(issues, true, "version", "unsupported_version", std::to_string(version));
                return false;
            }

            if (!root.contains("type"))
            {
                addIssue(issues, true, "type", "missing_required_field");
                return false;
            }
            if (!root["type"].is_string())
            {
                addIssue(issues, true, "type", "expected_string");
                return false;
            }
            if (root["type"].get<std::string>() != "Material")
            {
                addIssue(issues, true, "type", "expected_Material", root["type"].get<std::string>());
                return false;
            }

            return true;
        }

        bool readStringField(const json& root,
                             const char* key,
                             std::string& out_value,
                             std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!root[key].is_string())
            {
                addIssue(issues, true, key, "expected_string");
                return false;
            }

            out_value = root[key].get<std::string>();
            return true;
        }

        bool readFloatField(const json& root,
                            const char* key,
                            float& out_value,
                            std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!root[key].is_number())
            {
                addIssue(issues, true, key, "expected_number");
                return false;
            }

            out_value = root[key].get<float>();
            return true;
        }

        bool readBoolField(const json& root,
                           const char* key,
                           bool& out_value,
                           std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!root[key].is_boolean())
            {
                addIssue(issues, true, key, "expected_boolean");
                return false;
            }

            out_value = root[key].get<bool>();
            return true;
        }

        bool readVec4Field(const json& root,
                           const char* key,
                           glm::vec4& out_value,
                           std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!parseVec4(root[key], out_value))
            {
                addIssue(issues, true, key, "expected_vec4_number_array");
                return false;
            }

            return true;
        }

        bool readVec3Field(const json& root,
                           const char* key,
                           glm::vec3& out_value,
                           std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!parseVec3(root[key], out_value))
            {
                addIssue(issues, true, key, "expected_vec3_number_array");
                return false;
            }

            return true;
        }

        bool readTextureIdField(const json& root,
                                const char* key,
                                AssetID& out_id,
                                std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!parseAssetId(root[key], out_id))
            {
                addIssue(issues, true, key, "expected_asset_id");
                return false;
            }

            return true;
        }

        bool readImportNotes(const json& root,
                             const char* key,
                             std::vector<std::string>& out_notes,
                             std::vector<MaterialAssetIssue>& issues)
        {
            if (!root.contains(key))
                return true;
            if (!root[key].is_array())
            {
                addIssue(issues, true, key, "expected_string_array");
                return false;
            }

            for (const json& note : root[key])
            {
                if (!note.is_string())
                {
                    addIssue(issues, true, key, "expected_string_array");
                    return false;
                }
                out_notes.push_back(note.get<std::string>());
            }
            return true;
        }

        void writeTextureId(json& root, const char* key, AssetID id)
        {
            if (id.value != 0)
                root[key] = std::to_string(id.value);
        }
    }

    bool MaterialAssetParseResult::hasFatalIssue() const
    {
        for (const MaterialAssetIssue& issue : issues)
        {
            if (issue.fatal)
                return true;
        }
        return false;
    }

    MaterialAssetParseResult ParseMaterialAssetJson(const nlohmann::json& root)
    {
        MaterialAssetParseResult result{};
        if (!root.is_object())
        {
            addIssue(result.issues, true, {}, "expected_object");
            return result;
        }

        if (!validateSchema(root, result.issues))
            return result;

        MaterialAssetDesc& desc = result.desc;
        readStringField(root, "name", desc.name, result.issues);

        if (root.contains("workflow"))
        {
            if (!parseWorkflow(root["workflow"], desc.data.workflow))
                addIssue(result.issues, true, "workflow", "unsupported_workflow");
        }
        else
        {
            addIssue(result.issues, false, "workflow", "field_defaulted", "metallic_roughness");
        }

        readVec4Field(root, "base_color_factor", desc.data.base_color_factor, result.issues);
        readFloatField(root, "metallic_factor", desc.data.metallic_factor, result.issues);
        readFloatField(root, "roughness_factor", desc.data.roughness_factor, result.issues);
        readFloatField(root, "occlusion_strength", desc.data.occlusion_strength, result.issues);
        readVec3Field(root, "emissive_factor", desc.data.emissive_factor, result.issues);

        if (root.contains("alpha_mode") && !parseAlphaMode(root["alpha_mode"], desc.data.alpha_mode))
            addIssue(result.issues, true, "alpha_mode", "expected_alpha_mode");
        readBoolField(root, "double_sided", desc.data.double_sided, result.issues);
        readFloatField(root, "alpha_cutoff", desc.data.alpha_cutoff, result.issues);
        readFloatField(root, "normal_scale", desc.data.normal_scale, result.issues);

        readTextureIdField(root, "base_color_texture_id", desc.data.base_color_texture.texture, result.issues);
        readTextureIdField(root, "normal_texture_id", desc.data.normal_texture.texture, result.issues);
        readTextureIdField(root,
                           "metallic_roughness_texture_id",
                           desc.data.metallic_roughness_texture.texture,
                           result.issues);
        readTextureIdField(root, "occlusion_texture_id", desc.data.occlusion_texture.texture, result.issues);
        readTextureIdField(root, "emissive_texture_id", desc.data.emissive_texture.texture, result.issues);

        readStringField(root, "base_color_texture_path", desc.base_color_texture_path, result.issues);
        readStringField(root, "normal_texture_path", desc.normal_texture_path, result.issues);
        readStringField(root, "metallic_roughness_texture_path", desc.metallic_roughness_texture_path, result.issues);
        readStringField(root, "occlusion_texture_path", desc.occlusion_texture_path, result.issues);
        readStringField(root, "emissive_texture_path", desc.emissive_texture_path, result.issues);
        readStringField(root, "import_source", desc.import_source, result.issues);
        readImportNotes(root, "import_notes", desc.import_notes, result.issues);

        return result;
    }

    nlohmann::json SerializeMaterialAssetJson(const MaterialAssetDesc& desc)
    {
        json root;
        root["version"] = kMaterialAssetSchemaVersion;
        root["type"] = "Material";
        root["name"] = desc.name;
        root["workflow"] = workflowToString(desc.data.workflow);
        root["base_color_factor"] = {
            desc.data.base_color_factor.r,
            desc.data.base_color_factor.g,
            desc.data.base_color_factor.b,
            desc.data.base_color_factor.a};
        root["metallic_factor"] = desc.data.metallic_factor;
        root["roughness_factor"] = desc.data.roughness_factor;
        root["occlusion_strength"] = desc.data.occlusion_strength;
        root["emissive_factor"] = {
            desc.data.emissive_factor.r,
            desc.data.emissive_factor.g,
            desc.data.emissive_factor.b};
        root["alpha_mode"] = alphaModeToString(desc.data.alpha_mode);
        root["double_sided"] = desc.data.double_sided;
        root["alpha_cutoff"] = desc.data.alpha_cutoff;
        root["normal_scale"] = desc.data.normal_scale;

        writeTextureId(root, "base_color_texture_id", desc.data.base_color_texture.texture);
        writeTextureId(root, "normal_texture_id", desc.data.normal_texture.texture);
        writeTextureId(root, "metallic_roughness_texture_id", desc.data.metallic_roughness_texture.texture);
        writeTextureId(root, "occlusion_texture_id", desc.data.occlusion_texture.texture);
        writeTextureId(root, "emissive_texture_id", desc.data.emissive_texture.texture);

        root["base_color_texture_path"] = desc.base_color_texture_path;
        root["normal_texture_path"] = desc.normal_texture_path;
        root["metallic_roughness_texture_path"] = desc.metallic_roughness_texture_path;
        root["occlusion_texture_path"] = desc.occlusion_texture_path;
        root["emissive_texture_path"] = desc.emissive_texture_path;

        if (!desc.import_source.empty())
            root["import_source"] = desc.import_source;
        if (!desc.import_notes.empty())
            root["import_notes"] = desc.import_notes;

        return root;
    }
}
