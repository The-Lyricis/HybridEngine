#include "asset_registry.h"
#include <cctype>

namespace Hybrid
{
    void AssetRegistry::setRoot(const std::filesystem::path& root)
    {
        m_root = std::filesystem::absolute(root).lexically_normal();
    }
    const std::filesystem::path& AssetRegistry::getRoot() const { return m_root; }

    AssetID AssetRegistry::generateUniqueID()
    {
        std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
        for (;;)
        {
            AssetID id{ dist(m_rng) };
            if (m_by_id.find(id) == m_by_id.end())
                return id;
        }
    }

    std::string AssetRegistry::normalizeKey(const std::string &path) const
    {
        // 如果包含 ':' 且不是盘符（如 C:），视为逻辑路径，做简单统一：替换反斜杠为斜杠，移除重复 '/'
        auto colon_pos = path.find(':');
        if (colon_pos != std::string::npos && !(colon_pos == 1 && std::isalpha(static_cast<unsigned char>(path[0]))))
        {
            std::string norm = path;
            for (auto &ch : norm)
                if (ch == '\\')
                    ch = '/';
            // 去掉多余的连续 '/'
            std::string cleaned;
            cleaned.reserve(norm.size());
            bool prev_slash = false;
            for (char ch : norm)
            {
                if (ch == '/')
                {
                    if (!prev_slash)
                        cleaned.push_back(ch);
                    prev_slash = true;
                }
                else
                {
                    prev_slash = false;
                    cleaned.push_back(ch);
                }
            }
            return cleaned;
        }

        // 非逻辑路径：按文件系统规则规范化
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

        // 如果路径已存在且指向旧 ID，清理旧条目
        auto pit = m_by_path.find(key);
        if (pit != m_by_path.end() && pit->second != meta.id)
        {
            m_by_id.erase(pit->second);
        }

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
        auto it = m_by_path.find(normalizeKey(path));
        if (it == m_by_path.end())
            return nullptr;
        return find(it->second);
    }

    bool AssetRegistry::exists(AssetID id) const { return m_by_id.find(id) != m_by_id.end(); }

} // namespace Hybrid
