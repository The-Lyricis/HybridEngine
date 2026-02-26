#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "asset_registry.h"

namespace Hybrid
{
    struct AssetMetaLoadResult
    {
        uint32_t total_files = 0;
        uint32_t loaded = 0;
        uint32_t failed = 0;
    };

    class AssetMetaStore
    {
    public:
        explicit AssetMetaStore(std::shared_ptr<AssetRegistry> registry);

        AssetMetaLoadResult loadAll(const std::filesystem::path& assets_root);
        bool saveOne(const AssetMetadata& meta, const std::filesystem::path& assets_root);
        bool removeOne(const std::string& source_path, const std::filesystem::path& assets_root);

        static std::filesystem::path metaPathFromSource(const std::string& source_path,
                                                        const std::filesystem::path& assets_root);

    private:
        bool parseMetaFile(const std::filesystem::path& file, AssetMetadata& out) const;
        bool writeMetaFileAtomic(const std::filesystem::path& file, const AssetMetadata& meta) const;

        bool isValidLogicalPath(const std::string& path) const;

    private:
        std::shared_ptr<AssetRegistry> m_registry;
    };
} // namespace Hybrid
