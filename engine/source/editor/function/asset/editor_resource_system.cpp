#include "editor_resource_system.h"

#include "runtime/core/base/macro.h"
#include "editor/function/import/texture_importer.h"
#include "editor/function/import/mesh_importer.h"
#include "editor/function/import/audio_importer.h"
#include <unordered_set>

namespace Hybrid
{
    namespace
    {
        std::string toLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool isTextureExt(const std::string& extLower)
        {
            return extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" ||
                extLower == ".tga" || extLower == ".hdr" || extLower == ".bmp";
        }

        std::string makeAssetVPath(const std::filesystem::path& rel)
        {
            // 你的 VFS 是严格 alias:relative，不能写 asset:/xxx
            std::string r = rel.generic_string();
            while (!r.empty() && (r.front() == '/' || r.front() == '\\')) r.erase(r.begin());
            return std::string("asset:") + r;
        }

        // 简单 hash：文件大小 + mtime（足够让 importer 记录下来）
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
    }

    void EditorResourceSystem::scanAssetsAndAutoCreateTextureMeta()
    {
        if (!m_runtime || !m_importManager)
        {
            HBD_CORE_WARN("scanAssetsAndAutoCreateTextureMeta: runtime/importManager not ready");
            return;
        }

        auto registry = m_runtime->getRegistry();
        if (!registry)
        {
            HBD_CORE_ERROR("scanAssetsAndAutoCreateTextureMeta: registry is null");
            return;
        }

        const auto assetsRoot = registry->getRoot(); // 物理路径：GameProject/Assets
        if (assetsRoot.empty() || !std::filesystem::exists(assetsRoot))
        {
            HBD_CORE_ERROR("scanAssetsAndAutoCreateTextureMeta: assets root invalid: {}",
                assetsRoot.empty() ? "<empty>" : assetsRoot.string());
            return;
        }

        size_t created = 0;
        size_t skipped = 0;
        size_t existed = 0;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(assetsRoot, ec), end;
        if (ec)
        {
            HBD_CORE_ERROR("scanAssetsAndAutoCreateTextureMeta: cannot scan {} ({})",
                assetsRoot.string(), ec.message());
            return;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                HBD_CORE_WARN("scanAssetsAndAutoCreateTextureMeta: scan error ({})", ec.message());
                ec.clear();
                continue;
            }

            if (!it->is_regular_file())
                continue;

            const auto srcPhysical = it->path();

            // 跳过 meta 文件本身（*.meta）
            if (srcPhysical.extension() == ".meta")
                continue;

            const auto extLower = toLower(srcPhysical.extension().string());
            if (!isTextureExt(extLower))
            {
                skipped++;
                continue;
            }

            // meta 物理路径规则：source_file + ".meta"  =>  a.png.meta
            const std::filesystem::path metaPhysical = std::filesystem::path(srcPhysical.string() + ".meta");
            if (std::filesystem::exists(metaPhysical))
            {
                existed++;
                continue;
            }

            // 计算相对 Assets 的路径，构造 source vpath：asset:Textures/a.png
            auto rel = std::filesystem::relative(srcPhysical, assetsRoot, ec);
            if (ec)
            {
                HBD_CORE_WARN("scanAssets: relative() failed for {} ({})", srcPhysical.string(), ec.message());
                ec.clear();
                continue;
            }

            const std::string sourceVPath = makeAssetVPath(rel);

            // 若 registry 里已有记录，也不需要再创建（保险）
            if (registry->findByPath(sourceVPath) != nullptr)
            {
                existed++;
                continue;
            }

            ImportRequest req{};
            req.source_path = sourceVPath;
            req.cooked_path = ""; // 让 TextureImporter 自动生成 cache:Cooked/xxx.htex
            req.hash = makeSimpleHash(srcPhysical);
            req.preferred_type = AssetType::Texture2D;

            ImportResult result = m_importManager->importAsset(req);
            if (!result.success)
            {
                HBD_CORE_ERROR("Auto import failed for {} ({})", sourceVPath, result.message);
                continue;
            }

            // ImportManager 会通过 SaveMetaFn 回调到 saveAssetMeta(meta)，从而写 meta 文件
            created++;
            HBD_CORE_INFO("Auto-created meta: {} -> {}", sourceVPath, metaPhysical.string());
        }

        HBD_CORE_INFO("scanAssetsAndAutoCreateTextureMeta done. created={}, existed={}, skipped_non_texture={}",
            created, existed, skipped);
    }


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
        m_importManager = std::make_shared<ImportManager>(
            registry,
            vfs,
            [this](const AssetMetadata& meta) { return saveAssetMeta(meta); });

        registerDefaultImporters();
        scanAssetsAndAutoCreateTextureMeta();
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
} // namespace Hybrid
