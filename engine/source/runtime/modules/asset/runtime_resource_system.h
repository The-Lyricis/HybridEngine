#pragma once

#include <filesystem>
#include <memory>

#include "asset_manager.h"
#include "builtin_assets.h"
#include "asset_registry.h"
#include "asset_loader.h"
#include "asset_meta_store.h"
#include "runtime/core/base/vfs/virtual_file_system.h"
#include "runtime/modules/render/public/texture.h"
#include "material.h"
#include <runtime/modules/project/project_context.h>

namespace Hybrid
{
    // Runtime-only resource composition: VFS + registry + loaders + default assets.
    class RuntimeResourceSystem
    {
    public:
        void initialize(const ProjectContext& ctx,
            std::shared_ptr<IVirtualFileSystem> vfs); // Mount asset root, load meta index, init manager/loaders/defaults.

        std::shared_ptr<IVirtualFileSystem> getVFS() const { return m_vfs; }
        std::shared_ptr<AssetRegistry> getRegistry() const { return m_registry; }
        std::shared_ptr<AssetManager> getManager() const { return m_manager; }
        AssetID getBuiltinMeshID(BuiltinMesh mesh) const;
        void invalidateAsset(AssetID id) const;

    private:
        void registerDefaultLoaders(); // Register runtime loaders (texture/mesh/material).
        
        // Default runtime fallback assets.
        void createDefaultTexture();
        void createHybridDefaultMaterial();
        void createBuiltinMesh(BuiltinMesh mesh);

    private:
        std::shared_ptr<IVirtualFileSystem> m_vfs;      // Logical path -> native file access.
        std::shared_ptr<AssetRegistry>      m_registry; // Asset metadata index.
        std::shared_ptr<AssetManager>       m_manager;  // Runtime load/cache/state machine.
        std::unique_ptr<AssetMetaStore>     m_metaStore; // Meta file read path at startup.
        std::shared_ptr<Texture>            m_defaultTexture; // 1x1 white fallback texture.
        std::shared_ptr<Material>           m_hybridDefaultMaterial; // Fallback material.
        AssetID                             m_builtinCubeMeshId{}; // Built-in mesh ids, starting with cube.
        ProjectContext m_project; // Project context for path resolution and info.
    };
} // namespace Hybrid


