#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include "editor/services/asset/editor_resource_system.h"
#include "runtime/core/job/job_system.h"
#include "runtime/core/log/log_system.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/project/project_creator.h"
#include "runtime/modules/project/project_loader.h"

namespace
{
    int failures = 0;
#define EXPECT(value) do { if (!(value)) { std::cerr << "FAILED line " << __LINE__ << ": " #value "\n"; ++failures; } } while (false)

    bool waitForTerminal(Hybrid::EditorResourceSystem& resources,
                         const std::string& path,
                         Hybrid::ImportTaskState expected,
                         uint64_t* out_task_id = nullptr)
    {
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            resources.update(4, 2);
            for (const auto& task : resources.snapshotTasks())
            {
                if (task.source_path == path && task.state == expected)
                {
                    if (out_task_id)
                        *out_task_id = task.id;
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    bool waitForIdle(Hybrid::EditorResourceSystem& resources)
    {
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            resources.update(4, 2);
            bool active = false;
            for (const auto& task : resources.snapshotTasks())
                active |= task.state == Hybrid::ImportTaskState::Queued ||
                          task.state == Hybrid::ImportTaskState::Running;
            if (!active)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "hybrid_editor_service_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    Hybrid::ProjectCreateDesc description{};
    description.project_root = root;
    description.project_name = "AsyncImport";
    std::filesystem::path project_file;
    std::string error;
    EXPECT(Hybrid::ProjectCreator::CreateProject(description, project_file, error));

    const std::string valid_scene = R"({"meta":{"version":2},"environment":{"skyboxCubemap":0,"skyboxCubemapPath":"","skyboxIntensity":1.0,"skyboxRotationDegrees":0.0},"entities":[]})";
    { std::ofstream output(root / "Assets" / "Valid.scene"); output << valid_scene; }
    { std::ofstream output(root / "Assets" / "Broken.scene"); output << "not-json"; }

    Hybrid::ProjectContext project{};
    EXPECT(Hybrid::ProjectLoader::LoadFromFile(project_file, project, error));
    Hybrid::LogSystem::Config log_config{};
    const std::string log_path = (root / "test.log").string();
    log_config.logfile = log_path.c_str();
    Hybrid::LogSystem::initialize(log_config);

    auto jobs = std::make_shared<Hybrid::JobSystem>();
    EXPECT(jobs->initialize({2}));
    Hybrid::RuntimeResourceSystem runtime;
    EXPECT(runtime.initialize(project, nullptr, jobs));
    Hybrid::EditorResourceSystem editor;
    EXPECT(editor.initialize(runtime, jobs));

    const std::thread::id main_thread = std::this_thread::get_id();
    bool callback_on_main = false;
    editor.setAssetsReloadedCallback([&](const Hybrid::AssetsReloadedEvent&) {
        callback_on_main = std::this_thread::get_id() == main_thread;
    });

    editor.enqueueManualReimport("asset:Valid.scene");
    editor.enqueueManualReimport("asset:Valid.scene");
    EXPECT(waitForTerminal(editor, "asset:Valid.scene", Hybrid::ImportTaskState::Succeeded));
    size_t valid_task_count = 0;
    for (const auto& task : editor.snapshotTasks())
        valid_task_count += task.source_path == "asset:Valid.scene";
    EXPECT(valid_task_count == 1);
    EXPECT(callback_on_main);
    EXPECT(runtime.getRegistry()->findByPath("asset:Valid.scene").has_value());

    editor.enqueueManualReimport("asset:Valid.scene");
    editor.update(4, 2);
    { std::ofstream output(root / "Assets" / "Valid.scene", std::ios::app); output << ' '; }
    editor.enqueueSourceChanged("asset:Valid.scene");
    size_t running_count = 0;
    size_t queued_count = 0;
    for (const auto& task : editor.snapshotTasks())
    {
        running_count += task.source_path == "asset:Valid.scene" && task.state == Hybrid::ImportTaskState::Running;
        queued_count += task.source_path == "asset:Valid.scene" && task.state == Hybrid::ImportTaskState::Queued;
    }
    EXPECT(running_count == 1);
    EXPECT(queued_count == 1);
    EXPECT(waitForIdle(editor));
    valid_task_count = 0;
    for (const auto& task : editor.snapshotTasks())
        valid_task_count += task.source_path == "asset:Valid.scene";
    EXPECT(valid_task_count == 3);

    editor.enqueueManualReimport("asset:Broken.scene");
    uint64_t failed_id = 0;
    EXPECT(waitForTerminal(editor, "asset:Broken.scene", Hybrid::ImportTaskState::Failed, &failed_id));
    EXPECT(editor.retryTask(failed_id));
    editor.update(4, 2);
    editor.shutdown();
    for (const auto& task : editor.snapshotTasks())
        EXPECT(task.state != Hybrid::ImportTaskState::Queued && task.state != Hybrid::ImportTaskState::Running);

    runtime.shutdown();
    jobs->shutdown();
    Hybrid::LogSystem::shutdown();
    std::filesystem::remove_all(root, ignored);

    if (failures != 0)
    {
        std::cerr << failures << " editor service test assertion(s) failed\n";
        return 1;
    }
    std::cout << "HybridEditorServiceTests: all checks passed\n";
    return 0;
}
