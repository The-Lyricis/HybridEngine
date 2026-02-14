#pragma once

#include <filesystem>
#include <memory>

#include "asset_manager.h"
#include "asset_registry.h"
#include "asset_loader.h"
#include "opengl_texture2d_loader.h"
#include "runtime/core/base/virtual_file_system.h"

namespace Hybrid
{
    class ResourceSystem
    {
    public:
        void initialize();

        std::shared_ptr<IVirtualFileSystem> getVFS() const { return m_vfs; }
        std::shared_ptr<AssetRegistry> getRegistry() const { return m_registry; }
        std::shared_ptr<AssetManager> getManager() const { return m_manager; }

    private:
        void registerDefaultLoaders();

    private:
        std::shared_ptr<IVirtualFileSystem> m_vfs;
        std::shared_ptr<AssetRegistry>      m_registry;
        std::shared_ptr<AssetManager>       m_manager;
    };
} // namespace Hybrid

