#include "runtime_resource_system.h"

#include <filesystem>
#include <memory>

#include "builtin_assets.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/render/public/texture.h"
#include "opengl_texture2d_loader.h"
#include "mesh_loader.h"
#include "material_loader.h"
#include "scene_loader.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kRuntimeResourceLogTag = "[RuntimeResourceSystem]";

        std::string pathOrPlaceholder(const std::filesystem::path& path)
        {
            return path.empty() ? std::string("<empty>") : path.string();
        }
    } // namespace

    void RuntimeResourceSystem::initialize(const ProjectContext& ctx,
        std::shared_ptr<IVirtualFileSystem> vfs)
    {
        m_project = ctx;
        AssetMetaLoadResult meta_load_result{};

        HBD_CORE_INFO("{} initialize_started project_root={} assets_root={} cache_root={} build_root={}",
                      kRuntimeResourceLogTag,
                      pathOrPlaceholder(m_project.root),
                      pathOrPlaceholder(m_project.assets),
                      pathOrPlaceholder(m_project.cache),
                      pathOrPlaceholder(m_project.build));

        // 1) 浣跨敤澶栭儴娉ㄥ叆鐨?VFS锛涗负绌哄垯鍥為€€鍒涘缓鏈湴 VFS锛堥伩鍏嶇┖鎸囬拡宕╂簝锛?
        if (vfs)
        {
            m_vfs = std::move(vfs);
        }
        else
        {
            HBD_CORE_INFO("{} vfs_fallback_selected reason=null_injected_vfs implementation=NativeFileSystem",
                          kRuntimeResourceLogTag);
            m_vfs = std::make_shared<NativeFileSystem>();
        }

        // 2) Registry / MetaStore
        m_registry = std::make_shared<AssetRegistry>();
        m_metaStore = std::make_unique<AssetMetaStore>(m_registry);

        // 3) 浠?ProjectContext 涓哄噯锛欰ssets 蹇呴』鏈夋晥
        const auto& assetsRoot = m_project.assets;
        const auto& cacheRoot = m_project.cache;
        const auto& projRoot = m_project.root;
        const auto& buildRoot = m_project.build;

        if (assetsRoot.empty() || !std::filesystem::exists(assetsRoot))
        {
            HBD_CORE_ERROR("{} initialize_failed step=validate_assets_root path={} reason=invalid_assets_root",
                           kRuntimeResourceLogTag,
                           pathOrPlaceholder(assetsRoot));
            return;
        }

        // 4) 鎸傝浇锛氫繚鎸佷笌浣?NativeFileSystem 鐨勪弗鏍兼牸寮忎竴鑷达紙alias:relative锛?
        //    娉ㄦ剰锛歮ount 绗笁涓弬鏁版槸 priority
        m_vfs->mount("asset", assetsRoot, 0);

        if (!cacheRoot.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(cacheRoot, ec);
            if (ec)
            {
                HBD_CORE_WARN("{} mount_prepare_failed alias=cache path={} reason=create_directory_failed error={}",
                              kRuntimeResourceLogTag,
                              cacheRoot.string(),
                              ec.message());
            }
            m_vfs->mount("cache", cacheRoot, 0);
        }

        if (!projRoot.empty())
        {
            m_vfs->mount("project", projRoot, 0);
        }

        if (!buildRoot.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(buildRoot, ec);
            if (ec)
            {
                HBD_CORE_WARN("{} mount_prepare_failed alias=build path={} reason=create_directory_failed error={}",
                              kRuntimeResourceLogTag,
                              buildRoot.string(),
                              ec.message());
            }
            m_vfs->mount("build", buildRoot, 0);
        }

        // 5) Registry root 蹇呴』鎸囧悜椤圭洰 Assets锛圗ditorResourceSystem 淇濆瓨 meta 渚濊禆瀹冿級
        m_registry->setRoot(assetsRoot);

        HBD_CORE_INFO("{} mounts_ready project_root={} assets_root={} cache_root={} build_root={}",
                      kRuntimeResourceLogTag,
                      pathOrPlaceholder(projRoot),
                      pathOrPlaceholder(assetsRoot),
                      pathOrPlaceholder(cacheRoot),
                      pathOrPlaceholder(buildRoot));

        // 6) Meta 浠?Assets 涓嬪姞杞斤紙涓庝綘 Editor 淇濆瓨閫昏緫涓€鑷达級
        if (m_metaStore)
        {
            meta_load_result = m_metaStore->loadAll(assetsRoot);
            HBD_CORE_INFO("{} meta_load_completed assets_root={} total={} loaded={} failed={}",
                          kRuntimeResourceLogTag,
                          assetsRoot.string(),
                          meta_load_result.total_files,
                          meta_load_result.loaded,
                          meta_load_result.failed);
        }

        // 7) AssetManager
        m_manager = std::make_shared<AssetManager>(m_vfs, m_registry);

        registerDefaultLoaders();
        createDefaultTexture();
        createHybridDefaultMaterial();
        createBuiltinMesh(BuiltinMesh::Cube);

        HBD_CORE_INFO("{} initialize_completed meta_total={} meta_loaded={} meta_failed={} default_texture={} default_material={} builtin_cube_id={}",
                      kRuntimeResourceLogTag,
                      meta_load_result.total_files,
                      meta_load_result.loaded,
                      meta_load_result.failed,
                      m_defaultTexture ? "true" : "false",
                      m_hybridDefaultMaterial ? "true" : "false",
                      m_builtinCubeMeshId.value);
    }

    void RuntimeResourceSystem::registerDefaultLoaders()
    {
        auto texLoader = std::make_shared<GLTexture2DLoader>();
        m_manager->registerLoader<Texture>(texLoader);

        // Stub loaders for Mesh / Material锛堝悗缁浛鎹负瀹為檯瀹炵幇锛?
        m_manager->registerLoader<Mesh>(std::make_shared<MeshCookedLoader>());
        m_manager->registerLoader<Material>(std::make_shared<MaterialFileLoader>(m_registry));
        m_manager->registerLoader<Scene>(std::make_shared<SceneLoader>());
        HBD_CORE_DEBUG("{} loaders_registered count=4 types=Texture,Mesh,Material,Scene",
                       kRuntimeResourceLogTag);
    }

    void RuntimeResourceSystem::createDefaultTexture()
    {
        TextureDesc desc;
        desc.type = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width = 1;
        desc.height = 1;
        desc.layers = 1;
        desc.mipLevels = 1;

        const uint8_t white[4] = { 255, 255, 255, 255 };
        m_defaultTexture = Texture::Create(desc, white, sizeof(white));

        if (m_manager && m_defaultTexture)
        {
            m_manager->setDefault<Texture>(m_defaultTexture);
            HBD_CORE_INFO("{} default_texture_registered width={} height={} format=RGBA8",
                          kRuntimeResourceLogTag,
                          desc.width,
                          desc.height);
        }
        else
        {
            HBD_CORE_WARN("{} default_texture_register_skipped has_manager={} texture_created={}",
                          kRuntimeResourceLogTag,
                          m_manager ? "true" : "false",
                          m_defaultTexture ? "true" : "false");
        }
    }

    void RuntimeResourceSystem::createHybridDefaultMaterial()
    {
        MaterialData data;
        data.albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        data.metallic = 0.0f;
        data.roughness = 1.0f;
        data.ao = 1.0f;

        m_hybridDefaultMaterial = std::make_shared<Material>(data);
        if (m_manager && m_hybridDefaultMaterial)
        {
            m_manager->setDefault<Material>(m_hybridDefaultMaterial);
            HBD_CORE_INFO("{} default_material_registered name=HybridDefault",
                          kRuntimeResourceLogTag);
        }
        else
        {
            HBD_CORE_WARN("{} default_material_register_skipped has_manager={} material_created={}",
                          kRuntimeResourceLogTag,
                          m_manager ? "true" : "false",
                          m_hybridDefaultMaterial ? "true" : "false");
        }
    }

    AssetID RuntimeResourceSystem::getBuiltinMeshID(BuiltinMesh mesh) const
    {
        switch (mesh)
        {
        case BuiltinMesh::Cube:
            return m_builtinCubeMeshId;
        default:
            return {};
        }
    }

    void RuntimeResourceSystem::invalidateAsset(AssetID id) const
    {
        if (!m_manager || id.value == 0)
            return;
        m_manager->unload(id);
    }

    void RuntimeResourceSystem::createBuiltinMesh(BuiltinMesh mesh)
    {
        if (!m_registry || !m_manager)
            return;

        const char* logical_path = BuiltinAssets::meshPath(mesh);
        const char* logical_hash = BuiltinAssets::meshHash(mesh);
        if (!logical_path || logical_path[0] == '\0')
            return;

        AssetMetadata meta{};
        if (const auto* existing = m_registry->findByPath(logical_path))
        {
            meta = *existing;
        }
        else
        {
            meta.id = m_registry->generateUniqueID();
            meta.type = AssetType::Mesh;
            meta.source_path = logical_path;
            meta.cooked_path.clear();
            meta.hash = logical_hash ? logical_hash : "";
            meta.is_valid = true;
            m_registry->registerAsset(meta);
        }

        auto builtin_mesh = BuiltinAssets::createMesh(mesh);
        if (!builtin_mesh)
        {
            HBD_CORE_WARN("{} builtin_mesh_create_failed logical_path={} reason=create_mesh_failed",
                          kRuntimeResourceLogTag,
                          logical_path);
            return;
        }

        switch (mesh)
        {
        case BuiltinMesh::Cube:
            m_builtinCubeMeshId = meta.id;
            break;
        default:
            break;
        }

        m_manager->registerResident<Mesh>(meta.id, builtin_mesh);
        HBD_CORE_INFO("{} builtin_mesh_registered logical_path={} asset_id={}",
                      kRuntimeResourceLogTag,
                      logical_path,
                      meta.id.value);
    }

} // namespace Hybrid


