#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

namespace Hybrid
{
    class ProjectInstanceLock
    {
    public:
        ProjectInstanceLock() = default;
        ProjectInstanceLock(const ProjectInstanceLock&) = delete;
        ProjectInstanceLock& operator=(const ProjectInstanceLock&) = delete;
        ProjectInstanceLock(ProjectInstanceLock&& other) noexcept;
        ProjectInstanceLock& operator=(ProjectInstanceLock&& other) noexcept;
        ~ProjectInstanceLock();

        bool acquire(const std::filesystem::path& project_file, std::string& out_error);
        void release();

        bool isHeld() const;
        const std::filesystem::path& projectFile() const;

    private:
        std::intptr_t m_native_handle = -1;
        std::filesystem::path m_project_file;
    };
} // namespace Hybrid
