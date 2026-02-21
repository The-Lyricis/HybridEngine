#include "runtime_resource_system.h"

#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/texture.h"
#include "opengl_texture2d_loader.h"
#include "mesh_loader.h"
#include "material_loader.h"

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

    static std::filesystem::path CollectCacheRoot(const std::filesystem::path& asset_root)
    {
        if (asset_root.empty())
            return {};

        const auto candidate = asset_root.parent_path() / "cache";
        std::error_code ec;
        std::filesystem::create_directories(candidate, ec);
        if (ec)
            return {};

        return std::filesystem::weakly_canonical(candidate, ec);
    }

    void RuntimeResourceSystem::initialize()
    {
        m_vfs = std::make_shared<NativeFileSystem>();
        m_registry = std::make_shared<AssetRegistry>();
        m_metaStore = std::make_unique<AssetMetaStore>(m_registry);

        const auto assetRoot = CollectAssetRoot();

        if (assetRoot.empty())
        {
            HBD_CORE_ERROR("Asset root not found. Tried HYBRID_ROOT_DIR / HYBRID_PROJECT_ROOT_DIR.");
        }
        else
        {
            m_vfs->mount("asset", assetRoot, 0);
            if (const auto cacheRoot = CollectCacheRoot(assetRoot); !cacheRoot.empty())
            {
                m_vfs->mount("cache", cacheRoot, 0);
            }
            m_registry->setRoot(assetRoot);
            HBD_CORE_INFO("RuntimeResourceSystem using asset root: {}", assetRoot.string());

            if (m_metaStore)
            {
                const auto stat = m_metaStore->loadAll(assetRoot);
                HBD_CORE_INFO("Asset meta load: total={}, loaded={}, failed={}",
                              stat.total_files,
                              stat.loaded,
                              stat.failed);
            }
        }

        m_manager = std::make_shared<AssetManager>(m_vfs, m_registry);
        registerDefaultLoaders();
        createDefaultTexture();
        createDefaultMaterial();
        createDefaultMesh();
        HBD_CORE_TRACE("RuntimeResourceSystem initialized");
    }

    void RuntimeResourceSystem::registerDefaultLoaders()
    {
        auto texLoader = std::make_shared<GLTexture2DLoader>();
        m_manager->registerLoader<Texture>(texLoader);

        // Stub loaders for Mesh / Material（后续替换为实际实现）
        m_manager->registerLoader<Mesh>(std::make_shared<StubMeshLoader>());
        m_manager->registerLoader<Material>(std::make_shared<StubMaterialLoader>());
    }

    void RuntimeResourceSystem::createDefaultTexture()
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

    void RuntimeResourceSystem::createDefaultMaterial()
    {
        MaterialData data;
        data.albedo_color = {1.0f, 1.0f, 1.0f, 1.0f};
        data.metallic     = 0.0f;
        data.roughness    = 1.0f;
        data.ao           = 1.0f;

        m_defaultMaterial = std::make_shared<Material>(data);
        if (m_manager && m_defaultMaterial)
        {
            m_manager->setDefault<Material>(m_defaultMaterial);
        }
    }

    void RuntimeResourceSystem::createDefaultMesh()
    {
        m_defaultMesh = Mesh::CreateCube();
        if (m_manager && m_defaultMesh)
        {
            m_manager->setDefault<Mesh>(m_defaultMesh);
        }
    }
} // namespace Hybrid
