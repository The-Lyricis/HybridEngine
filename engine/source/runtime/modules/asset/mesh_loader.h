#pragma once
#include "asset_loader.h"
#include "mesh.h"

namespace Hybrid
{
    class MeshCookedLoader : public IAssetLoader<Mesh>
    {
    public:
        AssetType assetType() const override { return AssetType::Mesh; }
        std::shared_ptr<Mesh> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;
    };

    // Backward-compatible alias.
    using StubMeshLoader = MeshCookedLoader;
}
