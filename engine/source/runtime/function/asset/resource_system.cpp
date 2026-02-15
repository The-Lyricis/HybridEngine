#include "resource_system.h"

#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/texture.h"

namespace Hybrid
{
    static std::filesystem::path CollectAssetRoot()
    {
        std::vector<std::filesystem::path> roots;

#ifdef HYBRID_ROOT_DIR
        roots.push_back(std::filesystem::path(HYBRID_ROOT_DIR) / "asset");
#endif

#ifdef HYBRID_PROJECT_ROOT_DIR
        roots.push_back(std::filesystem::path(HYBRID_PROJECT_ROOT_DIR) / "engine" / "asset");
#endif

        for (auto& c : roots)
        {
            if (std::filesystem::exists(c))
            {
                return std::filesystem::canonical(c);
            }
        }
        return {};
    }

    void ResourceSystem::initialize()
    {
        m_vfs      = std::make_shared<NativeFileSystem>();
        m_registry = std::make_shared<AssetRegistry>();

        const auto assetRoot = CollectAssetRoot();

        if (assetRoot.empty())
        {
            HBD_CORE_ERROR("Asset root not found. Tried HYBRID_ROOT_DIR / HYBRID_PROJECT_ROOT_DIR.");
        }
        else
        {
            m_vfs->mount("asset", assetRoot, 0);
            m_registry->setRoot(assetRoot);
            HBD_CORE_INFO("ResourceSystem using asset root: {}", assetRoot.string());
        }

        m_manager = std::make_shared<AssetManager>(m_vfs, m_registry);
        registerDefaultLoaders();
        createDefaultTexture();
        HBD_CORE_TRACE("ResourceSystem initialized");
    }

    void ResourceSystem::registerDefaultLoaders()
    {
        auto texLoader = std::make_shared<GLTexture2DLoader>();
        m_manager->registerLoader<Texture>(texLoader);
    }

    void ResourceSystem::createDefaultTexture()
    {
        // 使用 Texture 工厂创建 1x1 白色纹理
        TextureDesc desc;
        desc.type   = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width  = 1;
        desc.height = 1;
        desc.layers = 1;
        desc.mipLevels = 1;

        const uint8_t white[4] = {255, 255, 255, 255};
        m_defaultTexture = Texture::Create(desc, white, sizeof(white));

        if (m_manager && m_defaultTexture)
        {
            m_manager->setDefault<Texture>(m_defaultTexture);
        }
    }
} // namespace Hybrid

