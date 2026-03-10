#include "file_dialogs_win32.h"

#ifdef _WIN32

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <shellapi.h>
#include <objbase.h>
#include <shobjidl.h>
#include <windows.h>

namespace Hybrid
{
    namespace
    {
        class ScopedComInit
        {
        public:
            ScopedComInit()
            {
                const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
                m_should_uninitialize = SUCCEEDED(hr);
            }

            ~ScopedComInit()
            {
                if (m_should_uninitialize)
                    CoUninitialize();
            }

        private:
            bool m_should_uninitialize = false;
        };

    }

    std::optional<std::filesystem::path> ShowSaveSceneDialogWin32(
        GLFWwindow* window,
        const std::filesystem::path& initial_directory,
        const std::wstring& default_file_name)
    {
        ScopedComInit com_init;

        IFileSaveDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
            return std::nullopt;

        DWORD options = 0;
        hr = dialog->GetOptions(&options);
        if (SUCCEEDED(hr))
            dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);

        const COMDLG_FILTERSPEC filters[] = {
            {L"Scene Files", L"*.scene"},
            {L"All Files", L"*.*"},
        };
        dialog->SetFileTypes(static_cast<UINT>(sizeof(filters) / sizeof(filters[0])), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"scene");

        if (!default_file_name.empty())
            dialog->SetFileName(default_file_name.c_str());

        IShellItem* folder = nullptr;
        if (!initial_directory.empty())
        {
            const std::wstring folder_wide = initial_directory.wstring();
            hr = SHCreateItemFromParsingName(folder_wide.c_str(), nullptr, IID_PPV_ARGS(&folder));
            if (SUCCEEDED(hr) && folder)
                dialog->SetFolder(folder);
        }

        HWND hwnd = window ? glfwGetWin32Window(window) : nullptr;
        hr = dialog->Show(hwnd);
        if (folder)
            folder->Release();

        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            dialog->Release();
            return std::nullopt;
        }

        if (FAILED(hr))
        {
            dialog->Release();
            return std::nullopt;
        }

        IShellItem* result = nullptr;
        hr = dialog->GetResult(&result);
        dialog->Release();
        if (FAILED(hr) || !result)
            return std::nullopt;

        PWSTR raw_path = nullptr;
        hr = result->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
        result->Release();
        if (FAILED(hr) || !raw_path)
            return std::nullopt;

        std::filesystem::path selected(raw_path);
        CoTaskMemFree(raw_path);
        return selected;
    }

    bool ShowInExplorerWin32(const std::filesystem::path& path)
    {
        if (path.empty() || !std::filesystem::exists(path))
            return false;

        const std::wstring wide = path.wstring();

        HINSTANCE result = nullptr;
        if (std::filesystem::is_directory(path))
        {
            result = ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else
        {
            const std::wstring args = L"/select,\"" + wide + L"\"";
            result = ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        }

        return reinterpret_cast<intptr_t>(result) > 32;
    }
}

#else

namespace Hybrid
{
    std::optional<std::filesystem::path> ShowSaveSceneDialogWin32(
        GLFWwindow*,
        const std::filesystem::path&,
        const std::wstring&)
    {
        return std::nullopt;
    }

    bool ShowInExplorerWin32(const std::filesystem::path&)
    {
        return false;
    }
}

#endif
