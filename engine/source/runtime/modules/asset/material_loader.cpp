#include "material_loader.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kMaterialLoaderLogTag = "[MaterialLoader]";

        using json = nlohmann::json;

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

        bool parseSurfaceMode(const json& node, MaterialSurfaceMode& out_mode)
        {
            if (node.is_string())
            {
                const std::string value = node.get<std::string>();
                if (value == "opaque")
                {
                    out_mode = MaterialSurfaceMode::Opaque;
                    return true;
                }
                if (value == "masked" || value == "alphatest" || value == "alpha_test")
                {
                    out_mode = MaterialSurfaceMode::Masked;
                    return true;
                }
                if (value == "transparent" || value == "alphablend" || value == "alpha_blend")
                {
                    out_mode = MaterialSurfaceMode::Transparent;
                    return true;
                }
                return false;
            }

            if (node.is_number_integer() || node.is_number_unsigned())
            {
                const int value = node.get<int>();
                if (value < static_cast<int>(MaterialSurfaceMode::Opaque) ||
                    value > static_cast<int>(MaterialSurfaceMode::Transparent))
                {
                    return false;
                }

                out_mode = static_cast<MaterialSurfaceMode>(value);
                return true;
            }

            return false;
        }

        void resolveTexturePathToId(const std::shared_ptr<AssetRegistry>& registry,
                                    const std::string& path,
                                    AssetID& out_id)
        {
            if (out_id.value != 0 || !registry || path.empty())
                return;

            const AssetMetadata* texture_meta = registry->findByPath(path);
            if (!texture_meta)
                return;

            if (texture_meta->type == AssetType::Texture2D || texture_meta->type == AssetType::TextureCube)
                out_id = texture_meta->id;
        }
    } // namespace

    std::shared_ptr<Material> MaterialFileLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        std::string load_path = meta.source_path;
        if (load_path.empty())
            load_path = meta.cooked_path;

        if (load_path.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} reason=empty_path",
                           kMaterialLoaderLogTag,
                           meta.id.value);
            return nullptr;
        }

        const std::vector<char> bytes = vfs.readAll(load_path);
        if (bytes.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} path={} reason=file_not_found",
                           kMaterialLoaderLogTag,
                           meta.id.value,
                           load_path);
            return nullptr;
        }

        json root = json::parse(bytes.begin(), bytes.end(), nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} path={} reason=invalid_json",
                           kMaterialLoaderLogTag,
                           meta.id.value,
                           load_path);
            return nullptr;
        }

        MaterialData data{};
        if (root.contains("albedo_color"))
            parseVec4(root["albedo_color"], data.albedo_color);
        if (root.contains("metallic") && root["metallic"].is_number())
            data.metallic = root["metallic"].get<float>();
        if (root.contains("roughness") && root["roughness"].is_number())
            data.roughness = root["roughness"].get<float>();
        if (root.contains("ao") && root["ao"].is_number())
            data.ao = root["ao"].get<float>();
        if (root.contains("emissive") && root["emissive"].is_number())
            data.emissive = root["emissive"].get<float>();
        if (root.contains("surface_mode"))
            (void)parseSurfaceMode(root["surface_mode"], data.surface_mode);
        if (root.contains("alpha_cutoff") && root["alpha_cutoff"].is_number())
            data.alpha_cutoff = root["alpha_cutoff"].get<float>();

        const auto parseIdIfExists = [&](const char* key, AssetID& out_id) {
            if (root.contains(key))
                (void)parseAssetId(root[key], out_id);
        };
        parseIdIfExists("albedo_map_id", data.albedo_map);
        parseIdIfExists("normal_map_id", data.normal_map);
        parseIdIfExists("metallic_roughness_map_id", data.metallic_roughness_map);
        parseIdIfExists("ao_map_id", data.ao_map);
        parseIdIfExists("emissive_map_id", data.emissive_map);

        std::string albedo_path;
        std::string normal_path;
        std::string mr_path;
        std::string ao_path;
        std::string emissive_path;
        const auto readPath = [&](const char* key, std::string& out_path) {
            if (root.contains(key) && root[key].is_string())
                out_path = root[key].get<std::string>();
        };
        readPath("albedo_map_path", albedo_path);
        readPath("normal_map_path", normal_path);
        readPath("metallic_roughness_map_path", mr_path);
        readPath("ao_map_path", ao_path);
        readPath("emissive_map_path", emissive_path);

        resolveTexturePathToId(m_registry, albedo_path, data.albedo_map);
        resolveTexturePathToId(m_registry, normal_path, data.normal_map);
        resolveTexturePathToId(m_registry, mr_path, data.metallic_roughness_map);
        resolveTexturePathToId(m_registry, ao_path, data.ao_map);
        resolveTexturePathToId(m_registry, emissive_path, data.emissive_map);

        return std::make_shared<Material>(data);
    }
} // namespace Hybrid
