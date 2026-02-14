#include "virtual_file_system.h"

#include <fstream>
#include <algorithm>

namespace Hybrid
{
    void NativeFileSystem::mount(const std::string& alias, const std::filesystem::path& root, int priority)
    {
        auto& vec = m_mounts[alias];
        vec.push_back({ root, priority });
        std::sort(vec.begin(), vec.end(), [](const VfsMount& a, const VfsMount& b) { return a.priority > b.priority; });
    }

    bool NativeFileSystem::exists(const std::string& path) const
    {
        return resolve(path).has_value();
    }

    std::optional<std::filesystem::path> NativeFileSystem::resolve(const std::string& path) const
    {
        // 约定路径形如 "<别名>:<相对路径>"
        auto pos = path.find(':');
        if (pos == std::string::npos || pos + 1 >= path.size())
            return std::nullopt;

        std::string alias = path.substr(0, pos);
        std::string rel = path.substr(pos + 1);
        auto it = m_mounts.find(alias);
        if (it == m_mounts.end())
            return std::nullopt;

        for (const auto& mount : it->second)
        {
            auto candidate = mount.root / rel;
            if (std::filesystem::exists(candidate))
                return candidate;
        }
        return std::nullopt;
    }

    std::vector<char> NativeFileSystem::readAll(const std::string& path) const
    {
        auto resolved = resolve(path);
        if (!resolved.has_value())
            return {};

        std::ifstream file(resolved.value(), std::ios::binary);
        if (!file)
            return {};

        std::vector<char> buffer(std::filesystem::file_size(resolved.value()));
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        return buffer;
    }
} // namespace Hybrid
