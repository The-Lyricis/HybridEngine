#pragma once

#include <filesystem>
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
        void* m_native_handle = nullptr;
        std::filesystem::path m_project_file;
    };
} // namespace Hybrid
