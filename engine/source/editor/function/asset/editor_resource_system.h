#pragma once

#include <memory>
#include <string>

#include "runtime/function/asset/resource_system.h"
#include "runtime/function/asset/asset_meta_store.h"
#include "editor/function/import/import_manager.h"

namespace Hybrid
{
    class EditorResourceSystem
    {
    public:
        bool initialize(const std::shared_ptr<ResourceSystem>& runtime_system);

        ImportResult importAsset(const ImportRequest& request);
        AssetID importTexture2D(const std::string& source_path,
                                const std::string& cooked_path = {},
                                const std::string& hash = {});
        bool saveAssetMeta(const AssetMetadata& meta);

        std::shared_ptr<ImportManager> getImportManager() const { return m_importManager; }

    private:
        void registerDefaultImporters();

    private:
        std::shared_ptr<ResourceSystem> m_runtime;
        std::unique_ptr<AssetMetaStore> m_metaStore;
        std::shared_ptr<ImportManager> m_importManager;
    };
} // namespace Hybrid
