#include "import_manager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kImportManagerLogTag = "[ImportManager]";
    } // namespace

    ImportManager::ImportManager(std::shared_ptr<AssetRegistry> registry,
                                 std::shared_ptr<IVirtualFileSystem> vfs,
                                 SaveMetaFn save_meta_fn)
        : m_registry(std::move(registry)), m_vfs(std::move(vfs)), m_save_meta_fn(std::move(save_meta_fn))
    {
    }

    void ImportManager::registerImporter(const std::shared_ptr<IAssetImporter>& importer)
    {
        if (!importer)
            return;
        m_importers.push_back(importer);
    }

    bool ImportManager::canImport(const std::string& source_path, AssetType preferred_type) const
    {
        if (source_path.empty())
            return false;

        const std::string ext = extractExtension(source_path);
        return static_cast<bool>(findImporter(preferred_type, ext));
    }

    ImportResult ImportManager::importAsset(const ImportRequest& request)
    {
        ImportResult result{};
        if (!m_registry)
        {
            result.message = "ImportManager: registry is null";
            return result;
        }
        if (request.source_path.empty())
        {
            result.message = "ImportManager: source_path is empty";
            return result;
        }
        if (!m_vfs)
        {
            result.message = "ImportManager: vfs is null";
            return result;
        }

        const std::string ext = extractExtension(request.source_path);
        const auto importer = findImporter(request.preferred_type, ext);
        if (!importer)
        {
            result.message = "ImportManager: no importer for extension/type";
            HBD_CORE_WARN("{} import_rejected source_path={} extension={} preferred_type={} reason=no_importer",
                          kImportManagerLogTag,
                          request.source_path,
                          ext.empty() ? "<none>" : ext,
                          static_cast<int>(request.preferred_type));
            return result;
        }

        HBD_CORE_DEBUG("{} import_started source_path={} extension={} preferred_type={} importer_type={}",
                       kImportManagerLogTag,
                       request.source_path,
                       ext.empty() ? "<none>" : ext,
                       static_cast<int>(request.preferred_type),
                       static_cast<int>(importer->primaryType()));
        result = importer->importAsset(request, *m_registry, *m_vfs);
        if (!result.success)
        {
            HBD_CORE_ERROR("{} import_failed source_path={} extension={} importer_type={} reason={}",
                           kImportManagerLogTag,
                           request.source_path,
                           ext.empty() ? "<none>" : ext,
                           static_cast<int>(importer->primaryType()),
                           result.message);
            return result;
        }

        if (m_save_meta_fn)
        {
            for (const auto& meta : result.assets)
            {
                if (!m_save_meta_fn(meta))
                {
                    result.success = false;
                    result.message = "ImportManager: save meta failed";
                    HBD_CORE_ERROR("{} import_failed source_path={} extension={} importer_type={} reason=save_meta_failed asset_id={} asset_source_path={}",
                                   kImportManagerLogTag,
                                   request.source_path,
                                   ext.empty() ? "<none>" : ext,
                                   static_cast<int>(importer->primaryType()),
                                   meta.id.value,
                                   meta.source_path);
                    return result;
                }
            }
        }
        else
        {
            // Fallback: in-memory registration only.
            for (const auto& meta : result.assets)
                m_registry->registerAsset(meta);
        }

        HBD_CORE_INFO("{} import_completed source_path={} extension={} importer_type={} assets={} primary_id={}",
                      kImportManagerLogTag,
                      request.source_path,
                      ext.empty() ? "<none>" : ext,
                      static_cast<int>(importer->primaryType()),
                      result.assets.size(),
                      result.primary_id.value);
        return result;
    }

    std::string ImportManager::extractExtension(const std::string& logical_path)
    {
        const auto pos = logical_path.find(':');
        const std::string relative = (pos == std::string::npos) ? logical_path : logical_path.substr(pos + 1);
        std::string ext = std::filesystem::path(relative).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext;
    }

    std::shared_ptr<IAssetImporter> ImportManager::findImporter(AssetType preferred_type, const std::string& ext) const
    {
        if (preferred_type != AssetType::Unknown)
        {
            for (const auto& importer : m_importers)
            {
                if (importer && importer->primaryType() == preferred_type &&
                    (ext.empty() || importer->supportsExtension(ext)))
                    return importer;
            }
        }

        for (const auto& importer : m_importers)
        {
            if (importer && importer->supportsExtension(ext))
                return importer;
        }
        return nullptr;
    }
} // namespace Hybrid
