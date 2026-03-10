#include "scene_loader.h"

#include "runtime/core/base/macro.h"
#include "runtime/modules/scene/scene_serializer.h"

namespace Hybrid
{
    std::shared_ptr<Scene> SceneLoader::loadFromLogicalPath(const std::string& logical, IVirtualFileSystem& vfs, const AssetRegistry* registry)
    {
        if (logical.empty())
        {
            HBD_CORE_ERROR("SceneLoader: empty logical path");
            return nullptr;
        }

        auto native = vfs.resolve(logical);
        if (!native)
        {
            HBD_CORE_ERROR("SceneLoader: resolve failed {}", logical);
            return nullptr;
        }

        auto scene = std::make_shared<Scene>();
        if (!SceneSerializer::DeserializeFromFile(*scene, *native, registry))
        {
            HBD_CORE_ERROR("SceneLoader: deserialize failed {}", native->string());
            return nullptr;
        }

        return scene;
    }

    std::shared_ptr<Scene> SceneLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        if (!meta.is_valid || meta.type != AssetType::Scene)
        {
            HBD_CORE_ERROR("SceneLoader: invalid meta or wrong type (id={})", meta.id.value);
            return nullptr;
        }

        // 运行时优先走 cooked；否则退回 source（便于调试）
        const std::string& logical = !meta.cooked_path.empty() ? meta.cooked_path : meta.source_path;
        if (logical.empty())
        {
            HBD_CORE_ERROR("SceneLoader: empty path (id={})", meta.id.value);
            return nullptr;
        }

        return loadFromLogicalPath(logical, vfs, nullptr);
    }
} // namespace Hybrid
