#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "editor/services/import/import_manager.h"
#include "runtime/modules/asset/asset_registry.h"

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
        using AssetsReloadedCallback = std::function<void(const AssetsReloadedEvent&)>;

        bool initialize(RuntimeResourceSystem& runtime_system);

        ImportResult importAsset(const ImportRequest& request);

        // Queue one logical source event (must be asset:relative).
        void enqueueSourceChanged(const std::string& source_vpath,
                                  AssetSourceChangeType change = AssetSourceChangeType::Modified);
        void enqueueManualReimport(const std::string& source_vpath);
        void setAssetsReloadedCallback(AssetsReloadedCallback callback);
        void clearAssetsReloadedCallback();

        // Consume queued import tasks with optional frame time budget.
        void processImportQueue(uint32_t max_jobs_per_frame = 2, uint32_t max_ms_budget = 0);

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
        };

        bool saveAssetMeta(const AssetMetadata& meta);
        void registerDefaultImporters();
        void enqueueRequest(const std::string& source_vpath,
                            AssetSourceChangeType change,
                            bool force_reimport,
                            bool high_priority);
        void emitAssetsReloaded(AssetsReloadedEvent event) const;

        bool processOneEvent(const std::string& source_vpath, AssetSourceChangeType change, bool force_reimport);
        bool handleUpsert(const std::string& source_vpath,
                          AssetSourceChangeType change,
                          bool force_reimport,
                          std::vector<AssetMetadata>* out_assets);
        bool handleRemove(const std::string& source_vpath, std::vector<AssetMetadata>* out_assets);

        static bool normalizeAssetLogicalPath(const std::string& input, std::string& out_path);
        static AssetSourceChangeType mergeChangeType(AssetSourceChangeType existing, AssetSourceChangeType incoming);
        static std::string buildDefaultCookedPath(const AssetMetadata& meta);
        static std::string makeSimpleHash(const std::filesystem::path& file);

    private:
        RuntimeResourceSystem* m_runtime = nullptr;
        std::unique_ptr<AssetMetaStore> m_metaStore;
        std::shared_ptr<ImportManager> m_importManager;
        std::deque<std::string> m_event_queue;
        std::unordered_map<std::string, PendingSourceChange> m_pending_changes;
        AssetsReloadedCallback m_assets_reloaded_callback;
        bool m_bootstrap_done = false;

        uint32_t m_min_settle_ms = 300;
    };
} // namespace Hybrid

