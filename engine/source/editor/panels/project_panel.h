#pragma once
#include "i_editor_panel.h"

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
        struct Entry
        {
            std::filesystem::path physical; // 物理路径
            std::filesystem::path rel;      // 相对 Assets 的路径
            bool is_dir = false;
        };

        void ensureRootInit();
        void renderToolbar();
        void renderDirectoryTree();
        void renderContent();

        void gatherEntries(const std::filesystem::path& dir);
        bool isHiddenFile(const std::filesystem::path& p) const;
        bool isMetaFile(const std::filesystem::path& p) const;

        std::string relToAssetVPath(const std::filesystem::path& rel) const;

    private:
        std::filesystem::path m_assetsRoot;
        bool m_rootInited = false;

        std::filesystem::path m_currentDir;
        std::vector<Entry> m_entries;

        std::string m_selectedRelStr;

        bool m_showMetaFiles = false;
    };
} // namespace Hybrid
