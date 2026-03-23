#include "editor_platform_services_win32.h"

#include "file_dialogs_win32.h"

#include <sstream>
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

namespace Hybrid
{
    namespace
    {
        std::wstring quoteArgument(const std::wstring& arg)
        {
            if (arg.find_first_of(L" \t\"") == std::wstring::npos)
                return arg;

            std::wstring quoted = L"\"";
            for (wchar_t ch : arg)
            {
                if (ch == L'"')
                    quoted += L'\\';
                quoted += ch;
            }
            quoted += L"\"";
            return quoted;
        }
    } // namespace

    std::filesystem::path EditorPlatformServicesWin32::getEditorUserDataDir() const
    {
        PWSTR raw_path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw_path)) && raw_path)
        {
            std::filesystem::path path(raw_path);
            CoTaskMemFree(raw_path);
            return path / "HybridEngine" / "Editor";
        }

        const wchar_t* local_app_data = _wgetenv(L"LOCALAPPDATA");
        if (local_app_data && *local_app_data)
            return std::filesystem::path(local_app_data) / "HybridEngine" / "Editor";

        return std::filesystem::temp_directory_path() / "HybridEngine" / "Editor";
    }

    std::filesystem::path EditorPlatformServicesWin32::getCurrentExecutablePath() const
    {
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length == 0 || length >= std::size(buffer))
            return {};
        return std::filesystem::path(buffer);
    }

    bool EditorPlatformServicesWin32::launchEditorProcess(const std::filesystem::path& editor_executable,
                                                          const std::vector<std::string>& args) const
    {
        if (editor_executable.empty())
            return false;

        std::wstringstream command_line;
        command_line << quoteArgument(editor_executable.wstring());
        for (const std::string& arg : args)
            command_line << L' ' << quoteArgument(std::filesystem::path(arg).wstring());

        std::wstring command = command_line.str();

        STARTUPINFOW startup_info{};
        startup_info.cb = sizeof(startup_info);
        PROCESS_INFORMATION process_info{};

        const BOOL created = CreateProcessW(
            editor_executable.wstring().c_str(),
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &startup_info,
            &process_info);
        if (!created)
            return false;

        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
        return true;
    }

    std::optional<std::filesystem::path>
    EditorPlatformServicesWin32::showSaveFileDialog(GLFWwindow* parent, const SaveFileDialogDesc& desc)
    {
        return ShowSaveFileDialogWin32(parent, desc);
    }

    std::vector<std::filesystem::path>
    EditorPlatformServicesWin32::showOpenFileDialog(GLFWwindow* parent, const OpenFileDialogDesc& desc)
    {
        return ShowOpenFileDialogWin32(parent, desc);
    }

    std::optional<std::filesystem::path>
    EditorPlatformServicesWin32::showSelectFolderDialog(GLFWwindow* parent, const SelectFolderDialogDesc& desc)
    {
        return ShowSelectFolderDialogWin32(parent, desc);
    }

    bool EditorPlatformServicesWin32::revealInFileBrowser(const std::filesystem::path& path)
    {
        return RevealInFileBrowserWin32(path);
    }
} // namespace Hybrid
