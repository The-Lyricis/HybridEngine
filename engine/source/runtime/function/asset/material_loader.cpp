#include "material_loader.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    //TODO:Finish real material loader
    std::shared_ptr<Material> StubMaterialLoader::load(const AssetMetadata& meta, IVirtualFileSystem& /*vfs*/)
    {
        HBD_CORE_WARN("Material loader stub not implemented, asset {} will fallback to default", meta.id.value);
        return nullptr; // fallback to default material if set
    }
} // namespace Hybrid
