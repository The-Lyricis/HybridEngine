#include "mesh_loader.h"

#include "mesh_cooked_format.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kMeshLoaderLogTag = "[MeshLoader]";
    } // namespace

    std::shared_ptr<Mesh> MeshCookedLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        if (meta.cooked_path.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=empty_cooked_path",
                           kMeshLoaderLogTag,
                           meta.id.value,
                           meta.source_path.empty() ? "<empty>" : meta.source_path);
            return nullptr;
        }

        const std::vector<char> bytes = vfs.readAll(meta.cooked_path);
        if (bytes.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} cooked_path={} reason=file_not_found",
                           kMeshLoaderLogTag,
                           meta.id.value,
                           meta.cooked_path);
            return nullptr;
        }

        auto mesh = std::make_shared<Mesh>();
        std::string decode_error;
        if (!HmeshDecode(bytes, *mesh, &decode_error))
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} cooked_path={} reason=decode_failed error={}",
                           kMeshLoaderLogTag,
                           meta.id.value,
                           meta.cooked_path,
                           decode_error.empty() ? "<empty>" : decode_error);
            return nullptr;
        }

        if (mesh->getVertices().empty() || mesh->getIndices().empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} cooked_path={} reason=empty_mesh_data",
                           kMeshLoaderLogTag,
                           meta.id.value,
                           meta.cooked_path);
            return nullptr;
        }

        return mesh;
    }
} // namespace Hybrid
