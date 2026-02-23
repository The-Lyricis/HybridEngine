#include "editor_resource_system.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "runtime/core/base/macro.h"
#include "runtime/function/asset/asset_registry.h"
#include "runtime/function/asset/asset_meta_store.h"
#include "runtime/function/asset/runtime_resource_system.h"

#include "editor/function/import/texture_importer.h"
#include "editor/function/import/mesh_importer.h"
#include "editor/function/import/audio_importer.h"

namespace Hybrid
{
    namespace
    {
        std::string toLower(std::string v)
        {
            std::transform(v.begin(), v.end(), v.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return v;
        }

        bool isTextureExt(const std::string& extLower)
        {
            return extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" ||
                extLower == ".tga" || extLower == ".hdr" || extLower == ".bmp";
        }

        std::string makeAssetVPath(const std::filesystem::path& rel)
        {
            // 你的 VFS 严格模式：alias:relative，relative 不能以 '/' 开头
            std::string r = rel.generic_string();
            while (!r.empty() && (r.front() == '/' || r.front() == '\\')) r.erase(r.begin());
            return std::string("asset:") + r;
        }

        // hash：size + mtime（足够用于增量导入判断）
        std::string makeSimpleHash(const std::filesystem::path& file)
        {
            std::error_code ec;

            auto sz = std::filesystem::file_size(file, ec);
            if (ec) sz = 0;

            ec.clear();
            auto ft = std::filesystem::last_write_time(file, ec);
            auto t = ec ? 0LL : (long long)ft.time_since_epoch().count();

            return std::to_string((unsigned long long)sz) + "_" + std::to_string(t);
        }
    } // namespace

    bool EditorResourceSystem::initialize(const std::shared_ptr<RuntimeResourceSystem>& runtime_system)
    {
        if (!runtime_system)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime resource system is null");
            return false;
        }

        auto registry = runtime_system->getRegistry();
        if (!registry)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime registry is null");
            return false;
        }

        m_runtime = runtime_system;
        m_metaStore = std::make_unique<AssetMetaStore>(registry);

        auto vfs = m_runtime->getVFS();
        if (!vfs)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime vfs is null");
            return false;
        }

        m_importManager = std::make_shared<ImportManager>(
            registry,
            vfs,
            [this](const AssetMetadata& meta) { return saveAssetMeta(meta); });

        registerDefaultImporters();


        return true;
    }

    ImportResult EditorResourceSystem::importAsset(const ImportRequest& request)
    {
        if (!m_importManager)
        {
            ImportResult out{};
            out.success = false;
            out.message = "EditorResourceSystem: import manager is not initialized";
            return out;
        }
        return m_importManager->importAsset(request);
    }

    AssetID EditorResourceSystem::importTexture2D(const std::string& source_path,
        const std::string& cooked_path,
        const std::string& hash)
    {
        ImportRequest request{};
        request.source_path = source_path;
        request.cooked_path = cooked_path;
        request.hash = hash;
        request.preferred_type = AssetType::Texture2D;

        ImportResult result = importAsset(request);
        if (!result.success)
        {
            HBD_CORE_ERROR("Editor import texture failed: {} ({})", source_path, result.message);
            return {};
        }
        return result.primary_id;
    }

    bool EditorResourceSystem::saveAssetMeta(const AssetMetadata& meta)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        auto registry = m_runtime->getRegistry();
        if (!registry)
            return false;

        const auto& asset_root = registry->getRoot();
        if (asset_root.empty())
        {
            HBD_CORE_ERROR("Editor save meta failed: asset root is empty");
            return false;
        }

        registry->registerAsset(meta);

        const bool ok = m_metaStore->saveOne(meta, asset_root);
        if (!ok)
        {
            HBD_CORE_ERROR("Editor save meta failed for {}", meta.source_path);
        }
        return ok;
    }

    void EditorResourceSystem::registerDefaultImporters()
    {
        if (!m_importManager)
            return;

        m_importManager->registerImporter(std::make_shared<TextureImporter>());
        m_importManager->registerImporter(std::make_shared<MeshImporter>());
        m_importManager->registerImporter(std::make_shared<AudioImporter>());
    }

    void EditorResourceSystem::tickAutoImport(float dt)
    {
        if (!m_autoImportEnabled)
            return;

        if (!m_runtime || !m_importManager)
            return;

        m_autoImportTimer += dt;
        if (m_autoImportTimer < m_autoImportInterval)
            return;

        m_autoImportTimer = 0.0f;
        scanAndAutoImportTexturesOnce();
    }

    void EditorResourceSystem::scanAndAutoImportTexturesOnce()
    {
        auto registry = m_runtime->getRegistry();
        if (!registry)
            return;

        const auto assetsRoot = registry->getRoot(); // 物理 Assets 根
        if (assetsRoot.empty() || !std::filesystem::exists(assetsRoot))
            return;

        size_t imported_new = 0;
        size_t imported_re = 0;
        size_t up_to_date = 0;
        size_t errors = 0;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(assetsRoot, ec), end;
        if (ec)
        {
            HBD_CORE_WARN("AutoImport: cannot scan {} ({})", assetsRoot.string(), ec.message());
            return;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!it->is_regular_file())
                continue;

            const auto srcPhysical = it->path();

            // 跳过 *.meta
            if (srcPhysical.extension() == ".meta")
                continue;

            const auto extLower = toLower(srcPhysical.extension().string());
            if (!isTextureExt(extLower))
                continue;

            // 物理 -> 相对 -> asset:vpath
            auto rel = std::filesystem::relative(srcPhysical, assetsRoot, ec);
            if (ec)
            {
                ec.clear();
                errors++;
                continue;
            }
            const std::string sourceVPath = makeAssetVPath(rel);

            // hash 计算（用于增量导入）
            const std::string newHash = makeSimpleHash(srcPhysical);

            // 快速跳过（已见且 hash 不变）
            auto seenIt = m_seenTextureHash.find(sourceVPath);
            if (seenIt != m_seenTextureHash.end() && seenIt->second == newHash)
            {
                up_to_date++;
                continue;
            }

            // registry 判定：无记录/无效/hash变化 -> import
            const AssetMetadata* existing = registry->findByPath(sourceVPath);

            bool needImport = false;
            bool isReimport = false;

            if (!existing)
            {
                needImport = true;
                isReimport = false;
            }
            else
            {
                if (!existing->is_valid || existing->hash != newHash)
                {
                    needImport = true;
                    isReimport = true;
                }
            }

            if (!needImport)
            {
                // registry 认为无需导入：同步缓存
                m_seenTextureHash[sourceVPath] = existing ? existing->hash : newHash;
                up_to_date++;
                continue;
            }

            ImportRequest req{};
            req.source_path = sourceVPath;
            req.cooked_path = "";      // 让 TextureImporter 默认生成 cache:Cooked/xxx.htex
            req.hash = newHash;  // 写入 meta.hash（增量导入关键）
            req.preferred_type = AssetType::Texture2D;

            ImportResult result = m_importManager->importAsset(req);
            if (!result.success)
            {
                HBD_CORE_ERROR("AutoImport: import failed for {} ({})", sourceVPath, result.message);
                errors++;
                continue;
            }

            // import 成功后 SaveMetaFn 已写出 *.meta
            m_seenTextureHash[sourceVPath] = newHash;

            if (isReimport) imported_re++;
            else imported_new++;
        }

        // 只有有动作/错误才打日志（避免刷屏）
        if (imported_new || imported_re || errors)
        {
            HBD_CORE_INFO("AutoImport tick: new={}, reimport={}, up_to_date={}, errors={}",
                imported_new, imported_re, up_to_date, errors);
        }
    }
} // namespace Hybrid
