#include "resource_system.h"

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    void ResourceSystem::initialize(const std::filesystem::path& projectRoot)
    {
        // 1) VFS 初始化并挂载 asset 目录
        m_vfs = std::make_shared<NativeFileSystem>();
        std::filesystem::path assetRoot = projectRoot / "engine" / "asset";
        m_vfs->mount("asset", assetRoot, /*priority*/ 0);

        // 2) Registry
        m_registry = std::make_shared<AssetRegistry>();
        m_registry->setRoot(assetRoot);

        // 3) AssetManager
        m_manager = std::make_shared<AssetManager>(m_vfs, m_registry);

        // 4) 默认 Loader 注册
        registerDefaultLoaders();

        HBD_CORE_INFO("ResourceSystem initialized. asset root = {}", assetRoot.string());
    }

    void ResourceSystem::registerDefaultLoaders()
    {
        // OpenGL Texture2D
        auto texLoader = std::make_shared<OpenglTexture2DLoader>();
        m_manager->registerLoader<Texture>(AssetType::Texture2D, texLoader);
    }
} // namespace Hybrid

