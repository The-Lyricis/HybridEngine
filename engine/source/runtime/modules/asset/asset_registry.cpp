#include "asset_registry.h"

#include <cctype>

namespace Hybrid
{
    namespace
    {
        void eraseSubassetMapping(std::unordered_map<std::string, AssetID>& by_subasset,
                                  AssetID id,
                                  AssetID parent_id,
                                  const std::string& subasset_key)
        {
            if (id.value == 0 || parent_id.value == 0 || subasset_key.empty())
                return;

            const std::string key = std::to_string(parent_id.value) + "|" + subasset_key;
            auto it = by_subasset.find(key);
            if (it != by_subasset.end() && it->second == id)
                by_subasset.erase(it);
        }
    } // namespace

    void AssetRegistry::setRoot(const std::filesystem::path& root)
    {
        m_root = std::filesystem::absolute(root).lexically_normal();
    }

    const std::filesystem::path& AssetRegistry::getRoot() const
    {
        return m_root;
    }

    AssetID AssetRegistry::generateUniqueID()
    {
        std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
        for (;;)
        {
            AssetID id{dist(m_rng)};
            if (m_by_id.find(id) == m_by_id.end())
                return id;
        }
    }

    std::string AssetRegistry::normalizeKey(const std::string& path) const
    {
        // Normalize logical path to alias:relative in strict mode.
        const auto colon_pos = path.find(':');
        const bool is_windows_drive =
            (colon_pos == 1 && !path.empty() && std::isalpha(static_cast<unsigned char>(path[0])));

        if (colon_pos != std::string::npos && !is_windows_drive)
        {
            const std::string alias = path.substr(0, colon_pos);
            std::string rel = path.substr(colon_pos + 1);

            if (rel.empty())
                return {};
            if (rel.front() == '/' || rel.front() == '\\')
                return {};

            for (char& ch : rel)
            {
                if (ch == '\\')
                    ch = '/';
            }

            std::filesystem::path rel_path(rel);
            rel_path = rel_path.lexically_normal();
            std::string norm_rel = rel_path.generic_string();

            while (norm_rel.rfind("./", 0) == 0)
                norm_rel.erase(0, 2);

            return alias + ":" + norm_rel;
        }

        // Non-logical path: normalize with filesystem semantics.
        std::filesystem::path p(path);
        if (!m_root.empty() && p.is_absolute())
        {
            std::error_code ec;
            auto rel = std::filesystem::relative(p, m_root, ec);
            if (!ec)
                p = rel;
        }

        p = p.lexically_normal();
        return p.generic_string();
    }

    std::string AssetRegistry::makeSubassetLookupKey(AssetID parent_id, const std::string& subasset_key) const
    {
        if (parent_id.value == 0 || subasset_key.empty())
            return {};
        return std::to_string(parent_id.value) + "|" + subasset_key;
    }

    void AssetRegistry::registerAsset(const AssetMetadata& meta)
    {
        if (meta.id.value == 0)
            return;

        const auto key = normalizeKey(meta.source_path);
        if (key.empty())
            return;

        // If this id already exists under another path, clear stale path mapping first.
        auto id_it = m_by_id.find(meta.id);
        if (id_it != m_by_id.end())
        {
            const auto old_key = normalizeKey(id_it->second.source_path);
            if (!old_key.empty() && old_key != key)
            {
                auto old_path_it = m_by_path.find(old_key);
                if (old_path_it != m_by_path.end() && old_path_it->second == meta.id)
                {
                    m_by_path.erase(old_path_it);
                }
            }

            const std::string old_subasset_key =
                makeSubassetLookupKey(id_it->second.parent_id, id_it->second.subasset_key);
            const std::string new_subasset_key = makeSubassetLookupKey(meta.parent_id, meta.subasset_key);
            if (!old_subasset_key.empty() && old_subasset_key != new_subasset_key)
                eraseSubassetMapping(m_by_subasset, meta.id, id_it->second.parent_id, id_it->second.subasset_key);
        }

        // If the path is re-registered with a new id, remove stale metadata entry.
        auto pit = m_by_path.find(key);
        if (pit != m_by_path.end() && pit->second != meta.id)
        {
            auto stale_it = m_by_id.find(pit->second);
            if (stale_it != m_by_id.end())
            {
                eraseSubassetMapping(m_by_subasset,
                                     stale_it->first,
                                     stale_it->second.parent_id,
                                     stale_it->second.subasset_key);
                m_by_id.erase(stale_it);
            }
        }

        m_by_path[key] = meta.id;
        const std::string subasset_key = makeSubassetLookupKey(meta.parent_id, meta.subasset_key);
        if (!subasset_key.empty())
            m_by_subasset[subasset_key] = meta.id;
        m_by_id[meta.id] = meta;
    }

    void AssetRegistry::unregisterAsset(AssetID id)
    {
        auto it = m_by_id.find(id);
        if (it != m_by_id.end())
        {
            m_by_path.erase(normalizeKey(it->second.source_path));
            eraseSubassetMapping(m_by_subasset, id, it->second.parent_id, it->second.subasset_key);
            m_by_id.erase(it);
        }
    }

    const AssetMetadata* AssetRegistry::find(AssetID id) const
    {
        auto it = m_by_id.find(id);
        return it == m_by_id.end() ? nullptr : &it->second;
    }

    const AssetMetadata* AssetRegistry::findByPath(const std::string& path) const
    {
        const auto key = normalizeKey(path);
        if (key.empty())
            return nullptr;

        auto it = m_by_path.find(key);
        if (it == m_by_path.end())
            return nullptr;

        return find(it->second);
    }

    const AssetMetadata* AssetRegistry::findBySubasset(AssetID parent_id, const std::string& subasset_key) const
    {
        const std::string lookup_key = makeSubassetLookupKey(parent_id, subasset_key);
        if (lookup_key.empty())
            return nullptr;

        auto it = m_by_subasset.find(lookup_key);
        if (it == m_by_subasset.end())
            return nullptr;

        return find(it->second);
    }

    bool AssetRegistry::exists(AssetID id) const
    {
        return m_by_id.find(id) != m_by_id.end();
    }

    std::vector<AssetMetadata> AssetRegistry::getAllAssets() const
    {
        std::vector<AssetMetadata> assets;
        assets.reserve(m_by_id.size());
        for (const auto& [id, meta] : m_by_id)
            assets.push_back(meta);
        return assets;
    }

} // namespace Hybrid
