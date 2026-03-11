#include "scene_loader.h"

#include "runtime/core/base/macro.h"
#include "runtime/modules/scene/scene_serializer.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kSceneLoaderLogTag = "[SceneLoader]";
    } // namespace

    std::shared_ptr<Scene> SceneLoader::loadFromLogicalPath(const std::string& logical, IVirtualFileSystem& vfs, const AssetRegistry* registry)
    {
        if (logical.empty())
        {
            HBD_CORE_ERROR("{} load_failed reason=empty_logical_path", kSceneLoaderLogTag);
            return nullptr;
        }

        auto native = vfs.resolve(logical);
        if (!native)
        {
            HBD_CORE_ERROR("{} load_failed logical_path={} reason=resolve_failed",
                           kSceneLoaderLogTag,
                           logical);
            return nullptr;
        }

        auto scene = std::make_shared<Scene>();
        if (!SceneSerializer::DeserializeFromFile(*scene, *native, registry))
        {
            HBD_CORE_ERROR("{} load_failed logical_path={} native_path={} reason=deserialize_failed",
                           kSceneLoaderLogTag,
                           logical,
                           native->string());
            return nullptr;
        }

        return scene;
    }

    std::shared_ptr<Scene> SceneLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        if (!meta.is_valid || meta.type != AssetType::Scene)
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} asset_type={} reason=invalid_metadata",
                           kSceneLoaderLogTag,
                           meta.id.value,
                           static_cast<uint32_t>(meta.type));
            return nullptr;
        }

        // 运行时优先走 cooked；否则退回 source（便于调试）
        const std::string& logical = !meta.cooked_path.empty() ? meta.cooked_path : meta.source_path;
        if (logical.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} reason=empty_path",
                           kSceneLoaderLogTag,
                           meta.id.value);
            return nullptr;
        }

        return loadFromLogicalPath(logical, vfs, nullptr);
    }
} // namespace Hybrid
