#pragma once

#include <filesystem>
#include <memory>

#include "asset_manager.h"
#include "asset_registry.h"
#include "asset_loader.h"
#include "asset_meta_store.h"
#include "runtime/core/base/virtual_file_system.h"
#include "mesh.h"
#include "material.h"
#include "texture.h"

namespace Hybrid
{
    class RuntimeResourceSystem
    {
    public:
        void initialize();

        std::shared_ptr<IVirtualFileSystem> getVFS() const { return m_vfs; }
        std::shared_ptr<AssetRegistry> getRegistry() const { return m_registry; }
        std::shared_ptr<AssetManager> getManager() const { return m_manager; }

    private:
        void registerDefaultLoaders();
        
        //Default
        void createDefaultTexture();
        void createDefaultMaterial();
        void createDefaultMesh();

    private:
        std::shared_ptr<IVirtualFileSystem> m_vfs;
        std::shared_ptr<AssetRegistry>      m_registry;
        std::shared_ptr<AssetManager>       m_manager;
        std::unique_ptr<AssetMetaStore>     m_metaStore;
        std::shared_ptr<Texture>            m_defaultTexture;
        std::shared_ptr<Material>           m_defaultMaterial;
        std::shared_ptr<Mesh>               m_defaultMesh;
    };
} // namespace Hybrid
