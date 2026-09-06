#include "tasks_panel.h"

#include "editor/core/context/editor_context.h"

namespace Hybrid
{
    namespace
    {
        const char* stateName(ImportTaskState state)
        {
            switch (state)
            {
            case ImportTaskState::Queued: return "Queued";
            case ImportTaskState::Running: return "Running";
            case ImportTaskState::Succeeded: return "Succeeded";
            case ImportTaskState::Failed: return "Failed";
            }
            return "Unknown";
        }
    }

    void TasksPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open)
            return;
        ImGui::Begin(getName(), &m_state.open);
        const auto tasks = ctx.asset_actions.list_import_tasks
            ? ctx.asset_actions.list_import_tasks()
            : std::vector<ImportTaskSnapshot>{};
        size_t active = 0;
        for (const auto& task : tasks)
            active += task.state == ImportTaskState::Queued || task.state == ImportTaskState::Running;
        ImGui::Text("Import tasks: %zu active / %zu total", active, tasks.size());

        if (ImGui::BeginTable("##ImportTasks", 5,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Asset");
            ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableHeadersRow();
            for (auto it = tasks.rbegin(); it != tasks.rend(); ++it)
            {
                const ImportTaskSnapshot& task = *it;
                ImGui::PushID(static_cast<int>(task.id));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(stateName(task.state));
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(task.source_path.c_str());
                if (!task.message.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", task.message.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(task.stage.c_str());
                ImGui::TableSetColumnIndex(3); ImGui::Text("%llu ms", static_cast<unsigned long long>(task.elapsed_ms));
                ImGui::TableSetColumnIndex(4);
                if (task.state == ImportTaskState::Failed && ImGui::SmallButton("Retry") && ctx.asset_actions.retry_import_task)
                    ctx.asset_actions.retry_import_task(task.id);
                if (task.state == ImportTaskState::Failed) ImGui::SameLine();
                if (ImGui::SmallButton("Show") && ctx.asset_actions.reveal_asset_source)
                    ctx.asset_actions.reveal_asset_source(task.source_path);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
}
