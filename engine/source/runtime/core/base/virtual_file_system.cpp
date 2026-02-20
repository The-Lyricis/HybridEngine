#include "virtual_file_system.h"

#include <algorithm>
#include <fstream>

namespace Hybrid
{
    namespace
    {
        bool hasParentTraversal(const std::filesystem::path& p)
        {
            for (const auto& part : p)
            {
                if (part == "..")
                    return true;
            }
            return false;
        }
    } // namespace

    void NativeFileSystem::mount(const std::string& alias, const std::filesystem::path& root, int priority)
    {
        auto& vec = m_mounts[alias];
        vec.push_back({root, priority});
        std::sort(vec.begin(), vec.end(), [](const VfsMount& a, const VfsMount& b) { return a.priority > b.priority; });
    }

    bool NativeFileSystem::exists(const std::string& path) const
    {
        return resolve(path).has_value();
    }

    std::optional<std::filesystem::path> NativeFileSystem::resolve(const std::string& path) const
    {
        // Expected format: alias:relative
        const auto pos = path.find(':');
        if (pos == std::string::npos || pos + 1 >= path.size())
            return std::nullopt;

        const std::string alias = path.substr(0, pos);
        std::string rel = path.substr(pos + 1);

        // Read-side compatibility for legacy alias:/relative paths.
        while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\'))
            rel.erase(rel.begin());

        if (rel.empty())
            return std::nullopt;

        for (char& ch : rel)
        {
            if (ch == '\\')
                ch = '/';
        }

        std::filesystem::path relPath(rel);
        if (relPath.is_absolute())
            return std::nullopt;

        relPath = relPath.lexically_normal();
        if (hasParentTraversal(relPath))
            return std::nullopt;

        auto it = m_mounts.find(alias);
        if (it == m_mounts.end())
            return std::nullopt;

        for (const auto& mount : it->second)
        {
            auto candidate = mount.root / relPath;
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
