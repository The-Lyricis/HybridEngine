#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "editor/services/import/import_manager.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/core/job/job_system.h"

namespace Hybrid
{
    class RuntimeResourceSystem;
    class AssetMetaStore;

    enum class AssetSourceChangeType : uint8_t
    {
        Added = 0,
        Modified,
        Removed
    };

    struct AssetsReloadedEvent
    {
        AssetSourceChangeType change = AssetSourceChangeType::Modified;
        std::string source_vpath;
        std::vector<AssetMetadata> assets;
    };

    class EditorResourceSystem
    {
    public:
        ~EditorResourceSystem() { shutdown(); }
        using AssetsReloadedCallback = std::function<void(const AssetsReloadedEvent&)>;

        bool initialize(RuntimeResourceSystem& runtime_system, std::shared_ptr<JobSystem> jobs = nullptr);
        void shutdown();

        ImportResult importAsset(const ImportRequest& request);

        // Queue one logical source event (must be asset:relative).
        void enqueueSourceChanged(const std::string& source_vpath,
                                  AssetSourceChangeType change = AssetSourceChangeType::Modified);
        void enqueueManualReimport(const std::string& source_vpath);
        void setAssetsReloadedCallback(AssetsReloadedCallback callback);
        void clearAssetsReloadedCallback();

        // Consume queued import tasks with optional frame time budget.
        void processImportQueue(uint32_t max_jobs_per_frame = 2, uint32_t max_ms_budget = 0);
        void update(uint32_t max_jobs_per_frame = 2, uint32_t max_ms_budget = 0)
        { processImportQueue(max_jobs_per_frame, max_ms_budget); }
        std::vector<ImportTaskSnapshot> snapshotTasks() const;
        bool retryTask(uint64_t task_id);

        // One-shot startup check: enqueue only missing meta/cooked assets.
        void bootstrapImportOnce();
        size_t reconcileMovedAssets();
        // Handle UI rename/move with stable AssetID.
        bool moveAsset(const std::string& old_source_vpath, const std::string& new_source_vpath);
        bool renameFolder(const std::string& old_folder_vpath, const std::string& new_folder_vpath);

    private:
        struct PendingSourceChange
        {
            AssetSourceChangeType type = AssetSourceChangeType::Modified;
            std::chrono::steady_clock::time_point last_event_time{};
            bool force_reimport = false;
            uint64_t task_id = 0;
        };

        struct RunningImport
        {
            uint64_t task_id = 0;
            AssetSourceChangeType change = AssetSourceChangeType::Modified;
            std::chrono::steady_clock::time_point started{};
            std::future<ImportPreparedResult> future;
        };

        bool saveAssetMeta(const AssetMetadata& meta);
        void registerDefaultImporters();
        void enqueueRequest(const std::string& source_vpath,
                            AssetSourceChangeType change,
                            bool force_reimport,
                            bool high_priority);
        void emitAssetsReloaded(AssetsReloadedEvent event) const;

        bool handleRemove(const std::string& source_vpath, std::vector<AssetMetadata>* out_assets);
        void collectCompletedImports();
        bool dispatchPendingImport(const std::string& source_vpath,
                                   const PendingSourceChange& pending);
        void updateTask(uint64_t id, ImportTaskState state, std::string stage, std::string message = {});

        static bool normalizeAssetLogicalPath(const std::string& input, std::string& out_path);
        static AssetSourceChangeType mergeChangeType(AssetSourceChangeType existing, AssetSourceChangeType incoming);
        static std::string buildDefaultCookedPath(const AssetMetadata& meta);
        static std::string makeSimpleHash(const std::filesystem::path& file);

    private:
        RuntimeResourceSystem* m_runtime = nullptr;
        std::unique_ptr<AssetMetaStore> m_metaStore;
        std::shared_ptr<ImportManager> m_importManager;
        std::shared_ptr<JobSystem> m_jobs;
        std::deque<std::string> m_event_queue;
        std::unordered_map<std::string, PendingSourceChange> m_pending_changes;
        std::unordered_map<std::string, RunningImport> m_running_imports;
        std::unordered_map<uint64_t, ImportTaskSnapshot> m_tasks;
        std::deque<uint64_t> m_task_order;
        AssetsReloadedCallback m_assets_reloaded_callback;
        bool m_bootstrap_done = false;
        bool m_accepting = false;
        uint64_t m_next_task_id = 1;

        uint32_t m_min_settle_ms = 300;
    };
} // namespace Hybrid

