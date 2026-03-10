#pragma once

#include "i_editor_panel.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace Hybrid
{
    struct EditorContext;

    class ProjectPanel final : public IEditorPanel
    {
    public:
        ProjectPanel() = default;

        const char* getName() const override { return "Project"; }
        void onImGuiRender(EditorContext& ctx) override;

    private:
        void drawWindowContextMenu(EditorContext& ctx) override;
        struct Entry
        {
            std::filesystem::path physical;
            std::filesystem::path rel;
            bool is_dir = false;
        };

        void ensureRootInit();
        void drawCommonContextMenu(EditorContext& ctx, const Entry* target);
        void renderToolbar(EditorContext& ctx);
        void renderDirectoryTree(EditorContext& ctx);
        void renderContent(EditorContext& ctx);
        void renderRenamePopup(EditorContext& ctx);
        void openCreateFolderPopup();
        void openCreateFolderPopup(std::filesystem::path target_dir);

        void gatherEntries(const std::filesystem::path& dir);
        bool isHiddenFile(const std::filesystem::path& p) const;
        bool isMetaFile(const std::filesystem::path& p) const;

        std::string relToAssetVPath(const std::filesystem::path& rel) const;
        void notifyAssetChange(EditorContext& ctx, const std::filesystem::path& rel, bool removed) const;
        void notifyAssetChangeRecursive(EditorContext& ctx,
                                        const std::filesystem::path& root,
                                        bool removed,
                                        std::vector<std::filesystem::path>* out_rel_files = nullptr) const;
        bool openEntry(EditorContext& ctx, const Entry& e);
        bool createFolder(const std::string& folder_name);
        bool duplicateEntry(EditorContext& ctx, const Entry& e);
        bool deleteEntry(EditorContext& ctx, const Entry& e);
        bool renameEntry(EditorContext& ctx, const std::filesystem::path& from, const std::filesystem::path& to);

    private:
        std::filesystem::path m_assetsRoot;
        bool m_rootInited = false;

        std::filesystem::path m_currentDir;
        std::vector<Entry> m_entries;

        std::string m_selectedRelStr;
        bool m_showMetaFiles = false;

        // Create-folder UI state.
        bool m_openCreateFolderPopup = false;
        char m_newFolderName[128] = "NewFolder";

        // Rename UI state.
        bool m_openRenamePopup = false;
        std::filesystem::path m_renameFrom;
        char m_renameInput[128] = {};

        // Auto-refresh current folder view for external file changes.
        std::chrono::steady_clock::time_point m_lastAutoRefresh{};
        float m_autoRefreshIntervalSec = 0.3f;
    };
} // namespace Hybrid
