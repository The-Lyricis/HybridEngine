#include "file_dialogs_win32.h"

#ifdef _WIN32

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <shellapi.h>
#include <objbase.h>
#include <shobjidl.h>
#include <vector>
#include <windows.h>

namespace Hybrid
{
    namespace
    {
        std::wstring toWide(const std::string& text)
        {
            return std::filesystem::path(text).wstring();
        }

        std::wstring defaultExtensionFromPattern(const std::string& pattern)
        {
            if (pattern.size() >= 3 && pattern.rfind("*.", 0) == 0)
                return toWide(pattern.substr(2));
            return L"";
        }

        std::wstring chooseDefaultExtension(const SaveFileDialogDesc& desc)
        {
            if (!desc.default_extension.empty())
                return toWide(desc.default_extension);
            if (!desc.filters.empty())
                return defaultExtensionFromPattern(desc.filters.front().pattern);
            return L"";
        }

        void buildFilterSpecs(const std::vector<FileDialogFilter>& desc_filters,
                              std::vector<std::wstring>& names,
                              std::vector<std::wstring>& patterns,
                              std::vector<COMDLG_FILTERSPEC>& out_specs)
        {
            names.clear();
            patterns.clear();
            out_specs.clear();
            names.reserve(desc_filters.size());
            patterns.reserve(desc_filters.size());
            out_specs.reserve(desc_filters.size());

            for (const auto& filter : desc_filters)
            {
                names.push_back(toWide(filter.name));
                patterns.push_back(toWide(filter.pattern));
                out_specs.push_back(COMDLG_FILTERSPEC{
                    names.back().c_str(),
                    patterns.back().c_str()
                });
            }
        }

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

    std::optional<std::filesystem::path> ShowSaveFileDialogWin32(
        GLFWwindow* window,
        const SaveFileDialogDesc& desc)
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

        if (!desc.title.empty())
            dialog->SetTitle(toWide(desc.title).c_str());

        std::vector<std::wstring> filter_names;
        std::vector<std::wstring> filter_patterns;
        std::vector<COMDLG_FILTERSPEC> filters;
        buildFilterSpecs(desc.filters, filter_names, filter_patterns, filters);
        if (!filters.empty())
        {
            dialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
            dialog->SetFileTypeIndex(1);
            const std::wstring default_extension = chooseDefaultExtension(desc);
            if (!default_extension.empty())
                dialog->SetDefaultExtension(default_extension.c_str());
        }

        if (!desc.default_name.empty())
            dialog->SetFileName(toWide(desc.default_name).c_str());

        IShellItem* folder = nullptr;
        if (!desc.initial_dir.empty())
        {
            const std::wstring folder_wide = desc.initial_dir.wstring();
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

    std::vector<std::filesystem::path> ShowOpenFileDialogWin32(
        GLFWwindow* window,
        const OpenFileDialogDesc& desc)
    {
        ScopedComInit com_init;
        std::vector<std::filesystem::path> results;

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
            return results;

        DWORD options = 0;
        hr = dialog->GetOptions(&options);
        if (SUCCEEDED(hr))
        {
            DWORD dialog_options = options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
            if (desc.allow_multi_select)
                dialog_options |= FOS_ALLOWMULTISELECT;
            dialog->SetOptions(dialog_options);
        }

        if (!desc.title.empty())
            dialog->SetTitle(toWide(desc.title).c_str());

        std::vector<std::wstring> filter_names;
        std::vector<std::wstring> filter_patterns;
        std::vector<COMDLG_FILTERSPEC> filters;
        buildFilterSpecs(desc.filters, filter_names, filter_patterns, filters);
        if (!filters.empty())
        {
            dialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
            dialog->SetFileTypeIndex(1);
        }

        IShellItem* folder = nullptr;
        if (!desc.initial_dir.empty())
        {
            const std::wstring folder_wide = desc.initial_dir.wstring();
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
            return results;
        }

        if (FAILED(hr))
        {
            dialog->Release();
            return results;
        }

        if (desc.allow_multi_select)
        {
            IShellItemArray* items = nullptr;
            hr = dialog->GetResults(&items);
            dialog->Release();
            if (FAILED(hr) || !items)
                return results;

            DWORD count = 0;
            items->GetCount(&count);
            results.reserve(static_cast<size_t>(count));
            for (DWORD i = 0; i < count; ++i)
            {
                IShellItem* item = nullptr;
                if (FAILED(items->GetItemAt(i, &item)) || !item)
                    continue;

                PWSTR raw_path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) && raw_path)
                {
                    results.emplace_back(raw_path);
                    CoTaskMemFree(raw_path);
                }
                item->Release();
            }
            items->Release();
            return results;
        }

        IShellItem* result = nullptr;
        hr = dialog->GetResult(&result);
        dialog->Release();
        if (FAILED(hr) || !result)
            return results;

        PWSTR raw_path = nullptr;
        hr = result->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
        result->Release();
        if (FAILED(hr) || !raw_path)
            return results;

        results.emplace_back(raw_path);
        CoTaskMemFree(raw_path);
        return results;
    }

    std::optional<std::filesystem::path> ShowSelectFolderDialogWin32(
        GLFWwindow* window,
        const SelectFolderDialogDesc& desc)
    {
        ScopedComInit com_init;

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
            return std::nullopt;

        DWORD options = 0;
        hr = dialog->GetOptions(&options);
        if (SUCCEEDED(hr))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

        if (!desc.title.empty())
            dialog->SetTitle(toWide(desc.title).c_str());

        IShellItem* folder = nullptr;
        if (!desc.initial_dir.empty())
        {
            const std::wstring folder_wide = desc.initial_dir.wstring();
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

    bool RevealInFileBrowserWin32(const std::filesystem::path& path)
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
    std::optional<std::filesystem::path> ShowSaveFileDialogWin32(
        GLFWwindow*,
        const SaveFileDialogDesc&)
    {
        return std::nullopt;
    }

    std::vector<std::filesystem::path> ShowOpenFileDialogWin32(
        GLFWwindow*,
        const OpenFileDialogDesc&)
    {
        return {};
    }

    std::optional<std::filesystem::path> ShowSelectFolderDialogWin32(
        GLFWwindow*,
        const SelectFolderDialogDesc&)
    {
        return std::nullopt;
    }

    bool RevealInFileBrowserWin32(const std::filesystem::path&)
    {
        return false;
    }
}

#endif
