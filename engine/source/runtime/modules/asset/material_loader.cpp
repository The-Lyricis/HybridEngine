#include "material_loader.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "material_asset_serializer.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kMaterialLoaderLogTag = "[MaterialLoader]";

        using json = nlohmann::json;

        void logIssue(const AssetMetadata& meta, const std::string& path, const MaterialAssetIssue& issue)
        {
            if (issue.fatal)
            {
                HBD_CORE_ERROR("{} load_failed asset_id={} path={} field={} reason={} value={}",
                               kMaterialLoaderLogTag,
                               meta.id.value,
                               path,
                               issue.field.empty() ? "<root>" : issue.field,
                               issue.reason,
                               issue.value.empty() ? "<none>" : issue.value);
            }
            else
            {
                HBD_CORE_WARN("{} schema_warning asset_id={} path={} field={} reason={} value={}",
                              kMaterialLoaderLogTag,
                              meta.id.value,
                              path,
                              issue.field.empty() ? "<root>" : issue.field,
                              issue.reason,
                              issue.value.empty() ? "<none>" : issue.value);
            }
        }

        void resolveTexturePathToId(const std::shared_ptr<AssetRegistry>& registry,
                                    const AssetMetadata& meta,
                                    const std::string& material_path,
                                    const char* field,
                                    const std::string& texture_path,
                                    AssetID& out_id)
        {
            if (out_id.value != 0 || texture_path.empty())
                return;
            if (!registry)
            {
                HBD_CORE_WARN("{} texture_path_unresolved asset_id={} path={} field={} texture_path={} reason=no_registry",
                              kMaterialLoaderLogTag,
                              meta.id.value,
                              material_path,
                              field,
                              texture_path);
                return;
            }

            const auto texture_meta = registry->findByPath(texture_path);
            if (!texture_meta)
            {
                HBD_CORE_WARN("{} texture_path_unresolved asset_id={} path={} field={} texture_path={} reason=not_registered",
                              kMaterialLoaderLogTag,
                              meta.id.value,
                              material_path,
                              field,
                              texture_path);
                return;
            }

            if (texture_meta->type == AssetType::Texture2D || texture_meta->type == AssetType::TextureCube)
            {
                out_id = texture_meta->id;
                return;
            }

            HBD_CORE_WARN("{} texture_path_unresolved asset_id={} path={} field={} texture_path={} reason=asset_type_mismatch",
                          kMaterialLoaderLogTag,
                          meta.id.value,
                          material_path,
                          field,
                          texture_path);
        }
    }

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

        MaterialAssetParseResult parsed = ParseMaterialAssetJson(root);
        for (const MaterialAssetIssue& issue : parsed.issues)
            logIssue(meta, load_path, issue);
        if (parsed.hasFatalIssue())
            return nullptr;

        MaterialAssetDesc& desc = parsed.desc;
        resolveTexturePathToId(m_registry,
                               meta,
                               load_path,
                               "base_color_texture_path",
                               desc.base_color_texture_path,
                               desc.data.base_color_texture.texture);
        resolveTexturePathToId(m_registry,
                               meta,
                               load_path,
                               "normal_texture_path",
                               desc.normal_texture_path,
                               desc.data.normal_texture.texture);
        resolveTexturePathToId(m_registry,
                               meta,
                               load_path,
                               "metallic_roughness_texture_path",
                               desc.metallic_roughness_texture_path,
                               desc.data.metallic_roughness_texture.texture);
        resolveTexturePathToId(m_registry,
                               meta,
                               load_path,
                               "occlusion_texture_path",
                               desc.occlusion_texture_path,
                               desc.data.occlusion_texture.texture);
        resolveTexturePathToId(m_registry,
                               meta,
                               load_path,
                               "emissive_texture_path",
                               desc.emissive_texture_path,
                               desc.data.emissive_texture.texture);

        return std::make_shared<Material>(desc.data);
    }
}
