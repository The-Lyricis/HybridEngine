#include "project_instance_lock.h"

#include <cstdint>
#include <sstream>
#include <utility>

#include <windows.h>

namespace Hybrid
{
    namespace
    {
        using MutexHandle = HANDLE;

        std::uint64_t fnv1a64(const std::string& text)
        {
            std::uint64_t value = 14695981039346656037ull;
            for (unsigned char ch : text)
            {
                value ^= static_cast<std::uint64_t>(ch);
                value *= 1099511628211ull;
            }
            return value;
        }

        std::wstring buildMutexName(const std::filesystem::path& project_file)
        {
            const std::string normalized = project_file.generic_string();
            const std::uint64_t hash = fnv1a64(normalized);

            std::wstringstream stream;
            stream << L"Local\\HybridEditorProject_" << std::hex << hash;
            return stream.str();
        }
    } // namespace

    ProjectInstanceLock::ProjectInstanceLock(ProjectInstanceLock&& other) noexcept
        : m_native_handle(std::exchange(other.m_native_handle, nullptr))
        , m_project_file(std::move(other.m_project_file))
    {
    }

    ProjectInstanceLock& ProjectInstanceLock::operator=(ProjectInstanceLock&& other) noexcept
    {
        if (this == &other)
            return *this;

        release();
        m_native_handle = std::exchange(other.m_native_handle, nullptr);
        m_project_file = std::move(other.m_project_file);
        return *this;
    }

    ProjectInstanceLock::~ProjectInstanceLock()
    {
        release();
    }

    bool ProjectInstanceLock::acquire(const std::filesystem::path& project_file, std::string& out_error)
    {
        release();

        if (project_file.empty())
        {
            out_error = "project file is empty";
            return false;
        }

        const std::wstring mutex_name = buildMutexName(project_file);
        MutexHandle handle = CreateMutexW(nullptr, FALSE, mutex_name.c_str());
        if (handle == nullptr)
        {
            out_error = "failed to create project instance mutex";
            return false;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(handle);
            out_error = "project is already open in another editor instance";
            return false;
        }

        m_native_handle = handle;
        m_project_file = project_file;
        return true;
    }

    void ProjectInstanceLock::release()
    {
        if (m_native_handle)
        {
            CloseHandle(static_cast<MutexHandle>(m_native_handle));
            m_native_handle = nullptr;
        }
        m_project_file.clear();
    }

    bool ProjectInstanceLock::isHeld() const
    {
        return m_native_handle != nullptr;
    }

    const std::filesystem::path& ProjectInstanceLock::projectFile() const
    {
        return m_project_file;
    }
} // namespace Hybrid
