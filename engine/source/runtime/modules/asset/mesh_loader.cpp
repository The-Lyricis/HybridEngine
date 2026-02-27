#include "mesh_loader.h"

#include "mesh_cooked_format.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    std::shared_ptr<Mesh> MeshCookedLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        if (meta.cooked_path.empty())
        {
            HBD_CORE_ERROR("Mesh load failed: cooked_path is empty for {}", meta.source_path);
            return nullptr;
        }

        const std::vector<char> bytes = vfs.readAll(meta.cooked_path);
        if (bytes.empty())
        {
            HBD_CORE_ERROR("Mesh load failed: cooked file not found {}", meta.cooked_path);
            return nullptr;
        }

        auto mesh = std::make_shared<Mesh>();
        std::string decode_error;
        if (!HmeshDecode(bytes, *mesh, &decode_error))
        {
            HBD_CORE_ERROR("Mesh load failed: decode {} ({})", meta.cooked_path, decode_error);
            return nullptr;
        }

        if (mesh->getVertices().empty() || mesh->getIndices().empty())
        {
            HBD_CORE_ERROR("Mesh load failed: cooked mesh is empty {}", meta.cooked_path);
            return nullptr;
        }

        return mesh;
    }
} // namespace Hybrid
