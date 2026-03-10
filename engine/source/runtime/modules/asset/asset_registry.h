#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "asset_type.h"

namespace Hybrid
{
    class AssetRegistry
    {
    public:
        void setRoot(const std::filesystem::path &root);
        const std::filesystem::path &getRoot() const;

        // 生成唯一 AssetID：随机 64-bit，并与当前表去重
        AssetID generateUniqueID();

        void registerAsset(const AssetMetadata &meta);
        void unregisterAsset(AssetID id);

        const AssetMetadata *find(AssetID id) const;
        const AssetMetadata *findByPath(const std::string &path) const;
        const AssetMetadata *findBySubasset(AssetID parent_id, const std::string& subasset_key) const;
        bool exists(AssetID id) const;
        std::vector<AssetMetadata> getAllAssets() const;

    private:
        std::string normalizeKey(const std::string &path) const;
        std::string makeSubassetLookupKey(AssetID parent_id, const std::string& subasset_key) const;

        std::filesystem::path m_root;
        std::unordered_map<AssetID, AssetMetadata, AssetID::Hasher> m_by_id;
        std::unordered_map<std::string, AssetID> m_by_path;
        std::unordered_map<std::string, AssetID> m_by_subasset;

        // 随机数发生器
        std::mt19937_64 m_rng{std::random_device{}()};
    };
} // namespace Hybrid
