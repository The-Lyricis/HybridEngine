#include "project_instance_lock.h"

#include <cstdint>
#include <sstream>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Hybrid
{
    namespace
    {
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

#ifdef _WIN32
        std::wstring buildMutexName(const std::filesystem::path& project_file)
        {
            const std::string normalized = project_file.generic_string();
            const std::uint64_t hash = fnv1a64(normalized);

            std::wstringstream stream;
            stream << L"Local\\HybridEditorProject_" << std::hex << hash;
            return stream.str();
        }
#endif
    } // namespace

    ProjectInstanceLock::ProjectInstanceLock(ProjectInstanceLock&& other) noexcept
        : m_native_handle(std::exchange(other.m_native_handle, -1))
        , m_project_file(std::move(other.m_project_file))
    {
    }

    ProjectInstanceLock& ProjectInstanceLock::operator=(ProjectInstanceLock&& other) noexcept
    {
        if (this == &other)
            return *this;

        release();
        m_native_handle = std::exchange(other.m_native_handle, -1);
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

#ifdef _WIN32
        const std::wstring mutex_name = buildMutexName(project_file);
        HANDLE handle = CreateMutexW(nullptr, FALSE, mutex_name.c_str());
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

        m_native_handle = reinterpret_cast<std::intptr_t>(handle);
#else
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(project_file, ec);
        const std::string normalized = (ec ? project_file : canonical).generic_string();
        const auto lock_path = std::filesystem::temp_directory_path() /
            ("HybridEditorProject_" + std::to_string(fnv1a64(normalized)) + ".lock");
        const int fd = ::open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd < 0)
        {
            out_error = "failed to open project instance lock: " + std::string(std::strerror(errno));
            return false;
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
        {
            const int lock_error = errno;
            ::close(fd);
            out_error = lock_error == EWOULDBLOCK
                ? "project is already open in another editor instance"
                : "failed to acquire project instance lock: " + std::string(std::strerror(lock_error));
            return false;
        }
        m_native_handle = fd;
#endif
        m_project_file = project_file;
        return true;
    }

    void ProjectInstanceLock::release()
    {
        if (m_native_handle != -1)
        {
#ifdef _WIN32
            CloseHandle(reinterpret_cast<HANDLE>(m_native_handle));
#else
            const int fd = static_cast<int>(m_native_handle);
            ::flock(fd, LOCK_UN);
            ::close(fd);
#endif
            m_native_handle = -1;
        }
        m_project_file.clear();
    }

    bool ProjectInstanceLock::isHeld() const
    {
        return m_native_handle != -1;
    }

    const std::filesystem::path& ProjectInstanceLock::projectFile() const
    {
        return m_project_file;
    }
} // namespace Hybrid
