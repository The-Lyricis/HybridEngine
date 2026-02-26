#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "i_asset_importer.h"

namespace Hybrid
{
    class ImportManager
    {
    public:
        using SaveMetaFn = std::function<bool(const AssetMetadata&)>;

        explicit ImportManager(std::shared_ptr<AssetRegistry> registry,
                               std::shared_ptr<IVirtualFileSystem> vfs,
                               SaveMetaFn save_meta_fn = {});

        void registerImporter(const std::shared_ptr<IAssetImporter>& importer);
        bool canImport(const std::string& source_path, AssetType preferred_type = AssetType::Unknown) const;
        ImportResult importAsset(const ImportRequest& request);

    private:
        static std::string extractExtension(const std::string& logical_path);
        std::shared_ptr<IAssetImporter> findImporter(AssetType preferred_type, const std::string& ext) const;

    private:
        std::shared_ptr<AssetRegistry> m_registry;
        std::shared_ptr<IVirtualFileSystem> m_vfs;
        SaveMetaFn m_save_meta_fn;
        std::vector<std::shared_ptr<IAssetImporter>> m_importers;
    };
} // namespace Hybrid
