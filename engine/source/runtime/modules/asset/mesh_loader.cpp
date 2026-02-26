#include "mesh_loader.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    std::shared_ptr<Mesh> StubMeshLoader::load(const AssetMetadata& meta, IVirtualFileSystem& /*vfs*/)
    {
        HBD_CORE_WARN("Mesh loader stub not implemented, asset {} will fallback to default", meta.id.value);
        return nullptr; // fallback to default mesh if set
    }
} // namespace Hybrid
