#include "project_panel.h"
#include "../editor_context.h"

#include <imgui.h>
#include <algorithm>
#include <system_error>

#include "runtime/core/base/macro.h"
#include "runtime/function/project/project_context.h" // ProjectService::Get()

namespace Hybrid
{
    void ProjectPanel::ensureRootInit()
    {
        if (m_rootInited)
            return;

        const auto& proj = ProjectService::Get();
        m_assetsRoot = proj.assets;

        if (m_assetsRoot.empty() || !std::filesystem::exists(m_assetsRoot))
        {
            HBD_CORE_WARN("ProjectPanel: assets root invalid: {}", m_assetsRoot.string());
            m_assetsRoot.clear();
            m_currentDir.clear();
            m_entries.clear();
            m_rootInited = true;
            return;
        }

        m_currentDir = m_assetsRoot;
        gatherEntries(m_currentDir);
        m_rootInited = true;
    }

    bool ProjectPanel::isMetaFile(const std::filesystem::path& p) const
    {
        return p.extension() == ".meta";
    }

    bool ProjectPanel::isHiddenFile(const std::filesystem::path& p) const
    {
        const auto name = p.filename().string();
        return !name.empty() && name[0] == '.';
    }

    std::string ProjectPanel::relToAssetVPath(const std::filesystem::path& rel) const
    {
        std::string r = rel.generic_string();
        while (!r.empty() && (r.front() == '/' || r.front() == '\\'))
            r.erase(r.begin());
        return std::string("asset:") + r;
    }

    void ProjectPanel::gatherEntries(const std::filesystem::path& dir)
    {
        m_entries.clear();

        if (m_assetsRoot.empty() || dir.empty())
            return;

        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec), end;
        if (ec)
        {
            HBD_CORE_WARN("ProjectPanel: cannot list dir {} ({})", dir.string(), ec.message());
            return;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            const auto p = it->path();
            const bool is_dir = it->is_directory();

            if (isHiddenFile(p))
                continue;

            if (!m_showMetaFiles && isMetaFile(p))
                continue;

            if (!is_dir && !it->is_regular_file())
                continue;

            Entry e{};
            e.physical = p;
            e.is_dir = is_dir;

            std::error_code rec;
            e.rel = std::filesystem::relative(p, m_assetsRoot, rec);
            if (rec)
                e.rel.clear();

            m_entries.push_back(std::move(e));
        }

        std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b)
            {
                if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
                return a.physical.filename().string() < b.physical.filename().string();
            });
    }

    void ProjectPanel::renderToolbar()
    {
        std::string pathLabel = "Assets";
        if (!m_assetsRoot.empty() && !m_currentDir.empty())
        {
            std::error_code ec;
            auto rel = std::filesystem::relative(m_currentDir, m_assetsRoot, ec);
            if (!ec && !rel.empty() && rel != ".")
                pathLabel += "/" + rel.generic_string();
        }

        ImGui::TextUnformatted(pathLabel.c_str());
        ImGui::SameLine();

        if (ImGui::Button("Up"))
        {
            if (!m_assetsRoot.empty() && m_currentDir != m_assetsRoot)
            {
                m_currentDir = m_currentDir.parent_path();
                gatherEntries(m_currentDir);
                m_selectedRelStr.clear();
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Refresh"))
        {
            gatherEntries(m_currentDir);
        }
        ImGui::SameLine();

        ImGui::Checkbox("Show .meta", &m_showMetaFiles);
    }

    void ProjectPanel::renderDirectoryTree()
    {
        if (m_assetsRoot.empty())
        {
            ImGui::TextUnformatted("Assets root not found.");
            return;
        }

        std::function<void(const std::filesystem::path&)> drawNode;
        drawNode = [&](const std::filesystem::path& dir)
            {
                const auto name = (dir == m_assetsRoot) ? std::string("Assets") : dir.filename().string();

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
                if (dir == m_currentDir)
                    flags |= ImGuiTreeNodeFlags_Selected;

                bool hasChildDir = false;
                {
                    std::error_code ec;
                    for (auto it = std::filesystem::directory_iterator(dir, ec);
                        it != std::filesystem::directory_iterator(); it.increment(ec))
                    {
                        if (ec) { ec.clear(); continue; }
                        if (it->is_directory())
                        {
                            const auto child = it->path();
                            if (isHiddenFile(child)) continue;
                            hasChildDir = true;
                            break;
                        }
                    }
                }
                if (!hasChildDir)
                    flags |= ImGuiTreeNodeFlags_Leaf;

                const bool opened = ImGui::TreeNodeEx(name.c_str(), flags);

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                {
                    m_currentDir = dir;
                    gatherEntries(m_currentDir);
                    m_selectedRelStr.clear();
                }

                if (opened)
                {
                    std::error_code ec;
                    std::vector<std::filesystem::path> children;
                    for (auto it = std::filesystem::directory_iterator(dir, ec);
                        it != std::filesystem::directory_iterator(); it.increment(ec))
                    {
                        if (ec) { ec.clear(); continue; }
                        if (!it->is_directory()) continue;
                        const auto child = it->path();
                        if (isHiddenFile(child)) continue;
                        children.push_back(child);
                    }

                    std::sort(children.begin(), children.end(),
                        [](const auto& a, const auto& b) { return a.filename().string() < b.filename().string(); });

                    for (const auto& c : children)
                        drawNode(c);

                    ImGui::TreePop();
                }
            };

        drawNode(m_assetsRoot);
    }

    void ProjectPanel::renderContent()
    {
        if (m_assetsRoot.empty())
            return;

        ImGui::Separator();

        for (const auto& e : m_entries)
        {
            const std::string name = e.physical.filename().string();
            const std::string relStr = e.rel.empty() ? std::string{} : e.rel.generic_string();

            const char* prefix = e.is_dir ? "[DIR] " : "      ";

            const bool selected = (!relStr.empty() && relStr == m_selectedRelStr);
            if (ImGui::Selectable((std::string(prefix) + name).c_str(), selected))
            {
                m_selectedRelStr = relStr;
            }

            if (e.is_dir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_currentDir = e.physical;
                gatherEntries(m_currentDir);
                m_selectedRelStr.clear();
            }

            if (ImGui::BeginPopupContextItem())
            {
                ImGui::TextUnformatted(name.c_str());
                ImGui::Separator();

                if (!e.is_dir && !e.rel.empty())
                {
                    const auto vpath = relToAssetVPath(e.rel);
                    ImGui::Text("VPath: %s", vpath.c_str());
                }

                if (ImGui::MenuItem("Refresh"))
                {
                    gatherEntries(m_currentDir);
                }

                ImGui::EndPopup();
            }
        }
    }

    void ProjectPanel::onImGuiRender(EditorContext&)
    {
        if (!m_open) return;

        ensureRootInit();

        ImGui::Begin(getName(), &m_open);

        renderToolbar();
        ImGui::Separator();

        ImGui::Columns(2, "ProjectColumns", true);

        ImGui::BeginChild("##ProjectTree");
        renderDirectoryTree();
        ImGui::EndChild();

        ImGui::NextColumn();

        ImGui::BeginChild("##ProjectContent");
        renderContent();
        ImGui::EndChild();

        ImGui::Columns(1);

        ImGui::End();
    }
} // namespace Hybrid
