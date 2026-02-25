#include "asset_registry.h"

#include <cctype>

namespace Hybrid
{
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
        }

        // If the path is re-registered with a new id, remove stale metadata entry.
        auto pit = m_by_path.find(key);
        if (pit != m_by_path.end() && pit->second != meta.id)
            m_by_id.erase(pit->second);

        m_by_path[key] = meta.id;
        m_by_id[meta.id] = meta;
    }

    void AssetRegistry::unregisterAsset(AssetID id)
    {
        auto it = m_by_id.find(id);
        if (it != m_by_id.end())
        {
            m_by_path.erase(normalizeKey(it->second.source_path));
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

    bool AssetRegistry::exists(AssetID id) const
    {
        return m_by_id.find(id) != m_by_id.end();
    }

} // namespace Hybrid
