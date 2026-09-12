#include "render_feature.h"

namespace Hybrid
{
    bool RenderGraphBlackboard::publish(const std::string& key, const std::string& resource_name)
    {
        if (key.empty() || resource_name.empty() || find(key).has_value())
            return false;
        m_entries.emplace_back(key, resource_name);
        return true;
    }

    std::optional<std::string> RenderGraphBlackboard::find(const std::string& key) const
    {
        for (const auto& entry : m_entries)
        {
            if (entry.first == key)
                return entry.second;
        }
        return std::nullopt;
    }

    void RenderGraphBlackboard::clear()
    {
        m_entries.clear();
    }
} // namespace Hybrid
