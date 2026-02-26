#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <unordered_map>

namespace Hybrid
{
    enum class FileWatcherChangeType : uint8_t
    {
        Added = 0,
        Modified,
        Removed
    };

    class PollingFileWatcher
    {
    public:
        using ChangeCallback = std::function<void(const std::filesystem::path&, FileWatcherChangeType)>;

        bool initialize(const std::filesystem::path& root, bool capture_existing = true)
        {
            m_root.clear();
            m_snapshot.clear();
            m_initialized = false;

            if (root.empty())
                return false;

            std::error_code ec;
            std::filesystem::path absolute_root = std::filesystem::absolute(root, ec);
            if (ec)
                return false;

            absolute_root = absolute_root.lexically_normal();
            if (!std::filesystem::exists(absolute_root) || !std::filesystem::is_directory(absolute_root))
                return false;

            m_root = std::move(absolute_root);
            m_initialized = true;

            if (capture_existing)
            {
                scan(m_snapshot);
            }

            return true;
        }

        void poll(const ChangeCallback& callback)
        {
            if (!m_initialized || !callback)
                return;

            std::unordered_map<std::string, Snapshot> current;
            scan(current);

            for (const auto& [key, cur] : current)
            {
                auto old_it = m_snapshot.find(key);
                if (old_it == m_snapshot.end())
                {
                    callback(cur.path, FileWatcherChangeType::Added);
                    continue;
                }

                const Snapshot& old = old_it->second;
                if (old.mtime != cur.mtime || old.size != cur.size)
                {
                    callback(cur.path, FileWatcherChangeType::Modified);
                }
            }

            for (const auto& [key, old] : m_snapshot)
            {
                if (current.find(key) == current.end())
                {
                    callback(old.path, FileWatcherChangeType::Removed);
                }
            }

            m_snapshot = std::move(current);
        }

        const std::filesystem::path& getRoot() const { return m_root; }
        bool isInitialized() const { return m_initialized; }

    private:
        struct Snapshot
        {
            std::filesystem::path path;
            std::filesystem::file_time_type mtime{};
            uintmax_t size = 0;
        };

        static bool isHiddenPath(const std::filesystem::path& p)
        {
            const auto name = p.filename().string();
            return !name.empty() && name[0] == '.';
        }

        static std::string makeKey(const std::filesystem::path& p)
        {
            std::error_code ec;
            std::filesystem::path absolute_path = std::filesystem::absolute(p, ec);
            if (ec)
                absolute_path = p;

            return absolute_path.lexically_normal().generic_string();
        }

        void scan(std::unordered_map<std::string, Snapshot>& out) const
        {
            out.clear();
            if (!m_initialized || m_root.empty())
                return;

            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(m_root, ec), end;
            if (ec)
                return;

            for (; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto& p = it->path();
                if (isHiddenPath(p))
                    continue;
                if (p.extension() == ".meta")
                    continue;
                if (!it->is_regular_file())
                    continue;

                Snapshot snap{};
                snap.path = p;

                snap.mtime = std::filesystem::last_write_time(p, ec);
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                snap.size = std::filesystem::file_size(p, ec);
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                out[makeKey(p)] = std::move(snap);
            }
        }

    private:
        std::filesystem::path m_root;
        std::unordered_map<std::string, Snapshot> m_snapshot;
        bool m_initialized = false;
    };
} // namespace Hybrid