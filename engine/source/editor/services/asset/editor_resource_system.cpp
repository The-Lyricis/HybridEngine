#include "editor_resource_system.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/asset_meta_store.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/asset/runtime_resource_system.h"

#include "editor/services/import/audio_importer.h"
#include "editor/services/import/mesh_importer.h"
#include "editor/services/import/texture_importer.h"
#include "editor/services/import/scene_importer.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kEditorResourceLogTag = "[EditorResourceSystem]";

        bool hasParentTraversal(const std::filesystem::path& p)
        {
            for (const auto& part : p)
            {
                if (part == "..")
                    return true;
            }
            return false;
        }

        std::string toLower(std::string v)
        {
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return v;
        }
        // Normalize logical path: trim, lowercase, ensure "asset:" prefix, and filter out invalid paths.
        bool isMetaLogicalPath(const std::string& logical_path)
        {
            const auto pos = logical_path.find(':');
            const std::string rel = (pos == std::string::npos) ? logical_path : logical_path.substr(pos + 1);
            return std::filesystem::path(rel).extension() == ".meta";
        }

        std::string inferAssetVPathFromMetaFile(const std::filesystem::path& meta_file,
                                                const std::filesystem::path& assets_root)
        {
            if (assets_root.empty() || meta_file.extension() != ".meta")
                return {};

            std::error_code ec;
            const std::filesystem::path source_physical = meta_file.parent_path() / meta_file.stem();
            if (!std::filesystem::exists(source_physical))
                return {};

            auto rel = std::filesystem::relative(source_physical, assets_root, ec);
            if (ec || rel.empty())
                return {};

            return std::string("asset:") + rel.generic_string();
        }

        const char* changeTypeName(AssetSourceChangeType change)
        {
            switch (change)
            {
            case AssetSourceChangeType::Added:
                return "added";
            case AssetSourceChangeType::Modified:
                return "modified";
            case AssetSourceChangeType::Removed:
                return "removed";
            default:
                return "unknown";
            }
        }
    } // namespace

    bool EditorResourceSystem::initialize(RuntimeResourceSystem& runtime_system)
    {
        auto registry = runtime_system.getRegistry();
        if (!registry)
        {
            HBD_CORE_ERROR("{} initialize_failed reason=runtime_registry_null", kEditorResourceLogTag);
            return false;
        }

        auto vfs = runtime_system.getVFS();
        if (!vfs)
        {
            HBD_CORE_ERROR("{} initialize_failed reason=runtime_vfs_null", kEditorResourceLogTag);
            return false;
        }

        m_runtime = &runtime_system;
        m_metaStore = std::make_unique<AssetMetaStore>(registry);
        m_importManager = std::make_shared<ImportManager>(
            registry,
            vfs,
            [this](const AssetMetadata& meta) { return saveAssetMeta(meta); });

        registerDefaultImporters();
        m_event_queue.clear();
        m_pending_changes.clear();
        m_assets_reloaded_callback = {};
        m_bootstrap_done = false;
        HBD_CORE_INFO("{} initialize_completed assets_root={}",
                      kEditorResourceLogTag,
                      registry->getRoot().empty() ? "<empty>" : registry->getRoot().string());
        return true;
    }

    ImportResult EditorResourceSystem::importAsset(const ImportRequest& request)
    {
        if (!m_importManager)
        {
            ImportResult out{};
            out.success = false;
            out.message = "EditorResourceSystem: import manager is not initialized";
            return out;
        }
        return m_importManager->importAsset(request);
    }

    // Queue one logical source event (must be asset:relative).
    // Implementation details:
    // - We normalize the logical path and filter out irrelevant events (e.g. non-importable files, meta files).
    // - We coalesce multiple events for the same asset within a short time window by merging their change types and updating the timestamp. This helps to reduce redundant imports during
    void EditorResourceSystem::enqueueSourceChanged(const std::string& source_vpath, AssetSourceChangeType change)
    {
        enqueueRequest(source_vpath, change, false, false);
    }

    void EditorResourceSystem::enqueueManualReimport(const std::string& source_vpath)
    {
        enqueueRequest(source_vpath, AssetSourceChangeType::Modified, true, true);
    }

    void EditorResourceSystem::setAssetsReloadedCallback(AssetsReloadedCallback callback)
    {
        m_assets_reloaded_callback = std::move(callback);
    }

    void EditorResourceSystem::clearAssetsReloadedCallback()
    {
        m_assets_reloaded_callback = {};
    }

    void EditorResourceSystem::enqueueRequest(const std::string& source_vpath,
                                              AssetSourceChangeType change,
                                              bool force_reimport,
                                              bool high_priority)
    {
        if (!m_importManager)
            return;

        std::string normalized;
        if (!normalizeAssetLogicalPath(source_vpath, normalized))
        {
            HBD_CORE_WARN("{} enqueue_rejected source_path={} reason=invalid_source_path",
                          kEditorResourceLogTag,
                          source_vpath);
            return;
        }
        // We allow only importable files to trigger events, which helps to reduce noise (e.g. temp files, irrelevant assets).
        if (isMetaLogicalPath(normalized))
            return;

        if (!m_importManager->canImport(normalized))
            return;

        const auto now = std::chrono::steady_clock::now();
        auto it = m_pending_changes.find(normalized);
        if (it == m_pending_changes.end())
        {
            PendingSourceChange pending{};
            pending.type = change;
            pending.force_reimport = force_reimport;
            pending.last_event_time =
                (high_priority && m_min_settle_ms > 0)
                    ? (now - std::chrono::milliseconds(m_min_settle_ms))
                    : now;
            m_pending_changes.emplace(normalized, pending);
            if (high_priority)
                m_event_queue.push_front(normalized);
            else
                m_event_queue.push_back(normalized);
            HBD_CORE_DEBUG("{} enqueue_added source_path={} change={} force_reimport={} high_priority={} queue_size={}",
                           kEditorResourceLogTag,
                           normalized,
                           changeTypeName(change),
                           force_reimport ? "true" : "false",
                           high_priority ? "true" : "false",
                           m_event_queue.size());
            return;
        }

        it->second.type = mergeChangeType(it->second.type, change);
        it->second.force_reimport = it->second.force_reimport || force_reimport;
        it->second.last_event_time =
            (high_priority && m_min_settle_ms > 0)
                ? (now - std::chrono::milliseconds(m_min_settle_ms))
                : now;

        if (high_priority)
        {
            m_event_queue.erase(std::remove(m_event_queue.begin(), m_event_queue.end(), normalized), m_event_queue.end());
            m_event_queue.push_front(normalized);
        }
        HBD_CORE_DEBUG("{} enqueue_merged source_path={} change={} force_reimport={} high_priority={} queue_size={}",
                       kEditorResourceLogTag,
                       normalized,
                       changeTypeName(it->second.type),
                       it->second.force_reimport ? "true" : "false",
                       high_priority ? "true" : "false",
                       m_event_queue.size());
    }

    // Consume queued import tasks with optional frame time budget.
    // One-shot startup check: enqueue only missing meta/cooked assets.
    // Implementation details:
    // - When processing events, we check the pending change type to decide how to handle it (upsert vs remove).
    // - For upsert, we attempt to import the asset and save its meta. For remove, we delete the meta and unregister from registry. 
    void EditorResourceSystem::processImportQueue(uint32_t max_jobs_per_frame, uint32_t max_ms_budget)
    {
        if (!m_runtime || !m_importManager || max_jobs_per_frame == 0 || m_event_queue.empty())
            return;

        const auto start = std::chrono::steady_clock::now();
        const size_t max_attempts = m_event_queue.size();

        uint32_t jobs = 0;
        size_t attempts = 0;
        while (!m_event_queue.empty() && jobs < max_jobs_per_frame && attempts < max_attempts)
        {
            if (max_ms_budget > 0)
            {
                const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                                            .count();
                if (elapsed_ms >= static_cast<int64_t>(max_ms_budget))
                    break;
            }

            std::string path = m_event_queue.front();
            m_event_queue.pop_front();
            ++attempts;

            auto pending_it = m_pending_changes.find(path);
            if (pending_it == m_pending_changes.end())
                continue;

            if (m_min_settle_ms > 0)
            {
                const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - pending_it->second.last_event_time)
                                        .count();
                if (age_ms < static_cast<int64_t>(m_min_settle_ms))
                {
                    m_event_queue.push_back(path);
                    continue;
                }
            }

            const AssetSourceChangeType change = pending_it->second.type;
            const bool force_reimport = pending_it->second.force_reimport;
            m_pending_changes.erase(pending_it);
            (void)processOneEvent(path, change, force_reimport);
            ++jobs;
        }
    }
    
    // One-shot startup check: enqueue only missing meta/cooked assets.
    void EditorResourceSystem::bootstrapImportOnce()
    {
        if (m_bootstrap_done || !m_runtime || !m_importManager)
            return;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return;

        const auto assets_root = registry->getRoot();
        if (assets_root.empty() || !std::filesystem::exists(assets_root))
            return;

        (void)reconcileMovedAssets();

        size_t queued_missing_meta = 0;
        size_t queued_missing_cooked = 0;
        size_t scanned = 0;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(assets_root, ec), end;
        if (ec)
        {
            HBD_CORE_WARN("{} bootstrap_scan_failed assets_root={} error={}",
                          kEditorResourceLogTag,
                          assets_root.string(),
                          ec.message());
            return;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!it->is_regular_file())
                continue;

            const auto& source_physical = it->path();
            if (source_physical.extension() == ".meta")
                continue;

            auto rel = std::filesystem::relative(source_physical, assets_root, ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            ++scanned;
            std::string source_vpath;
            if (!normalizeAssetLogicalPath(std::string("asset:") + rel.generic_string(), source_vpath))
                continue;

            if (!m_importManager->canImport(source_vpath))
                continue;

            bool need_enqueue = false;

            const auto meta_file = AssetMetaStore::metaPathFromSource(source_vpath, assets_root);
            const bool has_meta = !meta_file.empty() && std::filesystem::exists(meta_file);
            if (!has_meta)
            {
                ++queued_missing_meta;
                need_enqueue = true;
            }

            const AssetMetadata* existing = registry->findByPath(source_vpath);
            const bool cooked_exists = existing && !existing->cooked_path.empty() && vfs->exists(existing->cooked_path);
            if (!cooked_exists)
            {
                ++queued_missing_cooked;
                need_enqueue = true;
            }

            if (need_enqueue)
            {
                const AssetSourceChangeType change =
                    has_meta ? AssetSourceChangeType::Modified : AssetSourceChangeType::Added;
                enqueueSourceChanged(source_vpath, change);
            }
        }

        m_bootstrap_done = true;
        HBD_CORE_INFO("{} bootstrap_completed scanned={} queued_missing_meta={} queued_missing_cooked={}",
                      kEditorResourceLogTag,
                      scanned,
                      queued_missing_meta,
                      queued_missing_cooked);
    }

    bool EditorResourceSystem::moveAsset(const std::string& old_source_vpath, const std::string& new_source_vpath)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        std::string old_norm;
        std::string new_norm;
        if (!normalizeAssetLogicalPath(old_source_vpath, old_norm) ||
            !normalizeAssetLogicalPath(new_source_vpath, new_norm))
        {
            HBD_CORE_WARN("{} move_rejected old_source_path={} new_source_path={} reason=invalid_path",
                          kEditorResourceLogTag,
                          old_source_vpath,
                          new_source_vpath);
            return false;
        }

        if (old_norm == new_norm)
            return true;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return false;

        const auto* old_meta = registry->findByPath(old_norm);
        if (!old_meta)
        {
            // No old metadata entry: fallback to normal add/remove flow.
            enqueueSourceChanged(old_norm, AssetSourceChangeType::Removed);
            enqueueSourceChanged(new_norm, AssetSourceChangeType::Added);
            return false;
        }

        AssetMetadata moved = *old_meta;
        const std::string old_default_cooked = buildDefaultCookedPath(moved);
        moved.source_path = new_norm;
        moved.is_valid = true;

        if (auto source_physical = vfs->resolve(new_norm))
        {
            moved.hash = makeSimpleHash(*source_physical);
        }

        if (!old_default_cooked.empty() && moved.cooked_path == old_default_cooked)
        {
            const std::string new_default_cooked = buildDefaultCookedPath(moved);
            if (!new_default_cooked.empty())
                moved.cooked_path = new_default_cooked;
        }

        const auto& assets_root = registry->getRoot();
        if (assets_root.empty())
        {
            HBD_CORE_ERROR("{} move_failed old_source_path={} new_source_path={} reason=asset_root_empty",
                           kEditorResourceLogTag,
                           old_norm,
                           new_norm);
            return false;
        }

        // Persist new path first, then remove old meta file.
        if (!m_metaStore->saveOne(moved, assets_root))
        {
            HBD_CORE_ERROR("{} move_failed old_source_path={} new_source_path={} reason=save_new_meta_failed",
                           kEditorResourceLogTag,
                           old_norm,
                           moved.source_path);
            return false;
        }

        if (!m_metaStore->removeOne(old_norm, assets_root))
        {
            HBD_CORE_WARN("{} move_cleanup_failed source_path={} reason=remove_old_meta_failed",
                          kEditorResourceLogTag,
                          old_norm);
        }

        // Keep id stable, only update path/hash mapping.
        auto clear_pending = [this](const std::string& path) {
            m_pending_changes.erase(path);
            m_event_queue.erase(std::remove(m_event_queue.begin(), m_event_queue.end(), path), m_event_queue.end());
        };
        clear_pending(old_norm);
        clear_pending(new_norm);
        registry->registerAsset(moved);

        if (m_importManager && m_importManager->canImport(new_norm, moved.type))
            enqueueManualReimport(new_norm);

        HBD_CORE_INFO("{} move_completed old_source_path={} new_source_path={} asset_id={}",
                      kEditorResourceLogTag,
                      old_norm,
                      new_norm,
                      moved.id.value);
        return true;
    }

    bool EditorResourceSystem::renameFolder(const std::string& old_folder_vpath, const std::string& new_folder_vpath)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        std::string old_norm;
        std::string new_norm;
        if (!normalizeAssetLogicalPath(old_folder_vpath, old_norm) ||
            !normalizeAssetLogicalPath(new_folder_vpath, new_norm))
        {
            HBD_CORE_WARN("{} rename_folder_rejected old_path={} new_path={} reason=invalid_path",
                          kEditorResourceLogTag,
                          old_folder_vpath,
                          new_folder_vpath);
            return false;
        }

        if (old_norm == new_norm)
            return true;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return false;

        const auto& assets_root = registry->getRoot();
        if (assets_root.empty())
            return false;

        const std::string old_prefix = old_norm + "/";
        const std::string new_prefix = new_norm + "/";
        const auto assets = registry->getAllAssets();

        size_t moved_count = 0;
        for (auto meta : assets)
        {
            if (meta.source_path.rfind(old_prefix, 0) != 0)
                continue;

            const std::string old_source = meta.source_path;
            const std::string old_default_cooked = buildDefaultCookedPath(meta);
            meta.source_path = new_prefix + old_source.substr(old_prefix.size());

            if (auto source_physical = vfs->resolve(meta.source_path))
                meta.hash = makeSimpleHash(*source_physical);

            if (!old_default_cooked.empty() && meta.cooked_path == old_default_cooked)
            {
                const std::string new_default_cooked = buildDefaultCookedPath(meta);
                if (!new_default_cooked.empty())
                    meta.cooked_path = new_default_cooked;
            }

            if (!m_metaStore->saveOne(meta, assets_root))
            {
                HBD_CORE_WARN("{} rename_folder_failed new_source_path={} reason=save_meta_failed",
                              kEditorResourceLogTag,
                              meta.source_path);
                return false;
            }

            m_pending_changes.erase(old_source);
            m_pending_changes.erase(meta.source_path);
            m_event_queue.erase(std::remove(m_event_queue.begin(), m_event_queue.end(), old_source), m_event_queue.end());
            m_event_queue.erase(std::remove(m_event_queue.begin(), m_event_queue.end(), meta.source_path), m_event_queue.end());
            registry->registerAsset(meta);
            if (m_importManager && m_importManager->canImport(meta.source_path, meta.type))
                enqueueManualReimport(meta.source_path);
            ++moved_count;
        }

        HBD_CORE_INFO("{} rename_folder_completed old_path={} new_path={} updated_assets={}",
                      kEditorResourceLogTag,
                      old_norm,
                      new_norm,
                      moved_count);
        return true;
    }

    bool EditorResourceSystem::saveAssetMeta(const AssetMetadata& meta)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        auto registry = m_runtime->getRegistry();
        if (!registry)
            return false;

        const auto& asset_root = registry->getRoot();
        if (asset_root.empty())
        {
            HBD_CORE_ERROR("{} meta_save_failed source_path={} reason=asset_root_empty",
                           kEditorResourceLogTag,
                           meta.source_path.empty() ? "<empty>" : meta.source_path);
            return false;
        }

        std::string old_source_path;
        if (const auto* existing = registry->find(meta.id))
        {
            if (!existing->source_path.empty() && existing->source_path != meta.source_path)
                old_source_path = existing->source_path;
        }

        const bool ok = m_metaStore->saveOne(meta, asset_root);
        if (!ok)
        {
            HBD_CORE_ERROR("{} meta_save_failed source_path={} asset_id={} reason=save_one_failed",
                           kEditorResourceLogTag,
                           meta.source_path,
                           meta.id.value);
            return false;
        }

        if (!old_source_path.empty())
            (void)m_metaStore->removeOne(old_source_path, asset_root);

        registry->registerAsset(meta);
        return true;
    }

    void EditorResourceSystem::registerDefaultImporters()
    {
        if (!m_importManager)
            return;

        m_importManager->registerImporter(std::make_shared<TextureImporter>());
        m_importManager->registerImporter(std::make_shared<MeshImporter>());
        m_importManager->registerImporter(std::make_shared<AudioImporter>());
        m_importManager->registerImporter(std::make_shared<SceneImporter>());
    }

    size_t EditorResourceSystem::reconcileMovedAssets()
    {
        if (!m_runtime || !m_metaStore)
            return 0;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return 0;

        const auto& assets_root = registry->getRoot();
        if (assets_root.empty() || !std::filesystem::exists(assets_root))
            return 0;

        size_t reconciled = 0;
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(assets_root, ec), end;
        if (ec)
            return 0;

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!it->is_regular_file())
                continue;
            if (it->path().extension() != ".meta")
                continue;

            AssetMetadata meta{};
            if (!m_metaStore->loadOne(it->path(), meta))
                continue;

            if (meta.source_path.empty() || vfs->exists(meta.source_path))
                continue;

            std::string inferred_vpath = inferAssetVPathFromMetaFile(it->path(), assets_root);
            if (inferred_vpath.empty() || inferred_vpath == meta.source_path)
                continue;

            const std::string old_default_cooked = buildDefaultCookedPath(meta);
            meta.source_path = inferred_vpath;

            auto source_physical = vfs->resolve(inferred_vpath);
            if (source_physical)
                meta.hash = makeSimpleHash(*source_physical);

            if (!old_default_cooked.empty() && meta.cooked_path == old_default_cooked)
            {
                const std::string new_default_cooked = buildDefaultCookedPath(meta);
                if (!new_default_cooked.empty())
                    meta.cooked_path = new_default_cooked;
            }

            if (!m_metaStore->saveOne(meta, assets_root))
            {
                HBD_CORE_WARN("{} reconcile_failed inferred_source_path={} reason=save_meta_failed",
                              kEditorResourceLogTag,
                              inferred_vpath);
                continue;
            }

            registry->registerAsset(meta);
            if (m_importManager && m_importManager->canImport(meta.source_path, meta.type))
                enqueueManualReimport(meta.source_path);
            ++reconciled;
            HBD_CORE_INFO("{} reconcile_completed meta_path={} inferred_source_path={}",
                          kEditorResourceLogTag,
                          it->path().string(),
                          inferred_vpath);
        }

        if (reconciled > 0)
            HBD_CORE_INFO("{} reconcile_summary reconciled_assets={}",
                          kEditorResourceLogTag,
                          reconciled);

        return reconciled;
    }

    void EditorResourceSystem::emitAssetsReloaded(AssetsReloadedEvent event) const
    {
        if (!m_assets_reloaded_callback || event.assets.empty())
            return;

        m_assets_reloaded_callback(event);
    }

    bool EditorResourceSystem::processOneEvent(const std::string& source_vpath,
                                               AssetSourceChangeType change,
                                               bool force_reimport)
    {
        std::vector<AssetMetadata> changed_assets;
        bool handled = false;
        switch (change)
        {
        case AssetSourceChangeType::Added:
        case AssetSourceChangeType::Modified:
            handled = handleUpsert(source_vpath, change, force_reimport, &changed_assets);
            break;
        case AssetSourceChangeType::Removed:
            handled = handleRemove(source_vpath, &changed_assets);
            break;
        default:
            return false;
        }

        if (handled && !changed_assets.empty())
        {
            emitAssetsReloaded(AssetsReloadedEvent{
                change,
                source_vpath,
                std::move(changed_assets),
            });
        }
        return handled;
    }

    bool EditorResourceSystem::handleUpsert(const std::string& source_vpath,
                                            AssetSourceChangeType change,
                                            bool force_reimport,
                                            std::vector<AssetMetadata>* out_assets)
    {
        if (!m_runtime || !m_importManager)
            return false;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return false;

        if (!vfs->exists(source_vpath))
        {
            // File disappeared before import starts.
            return handleRemove(source_vpath, out_assets);
        }

        auto source_physical = vfs->resolve(source_vpath);
        if (!source_physical)
            return false;

        const std::string new_hash = makeSimpleHash(*source_physical);
        const AssetMetadata* existing = registry->findByPath(source_vpath);
        const bool cooked_exists =
            existing && !existing->cooked_path.empty() && vfs->exists(existing->cooked_path);

        if (!force_reimport && existing && existing->is_valid && existing->hash == new_hash && cooked_exists)
        {
            return true;
        }

        ImportRequest req{};
        req.source_path = source_vpath;
        req.hash = new_hash;
        req.preferred_type = existing ? existing->type : AssetType::Unknown;

        ImportResult result = m_importManager->importAsset(req);
        if (!result.success)
        {
            HBD_CORE_ERROR("{} import_failed source_path={} change={} force_reimport={} reason={}",
                           kEditorResourceLogTag,
                           source_vpath,
                           changeTypeName(change),
                           force_reimport ? "true" : "false",
                           result.message);
            return false;
        }
        if (out_assets)
            *out_assets = result.assets;

        HBD_CORE_INFO("{} import_completed source_path={} change={} force_reimport={} assets={}",
                      kEditorResourceLogTag,
                      source_vpath,
                      changeTypeName(change),
                      force_reimport ? "true" : "false",
                      result.assets.size());
        return true;
    }

    bool EditorResourceSystem::handleRemove(const std::string& source_vpath, std::vector<AssetMetadata>* out_assets)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return false;

        const auto* existing = registry->findByPath(source_vpath);
        const std::filesystem::path assets_root = registry->getRoot();
        if (existing && out_assets)
            out_assets->push_back(*existing);

        bool removed_meta = false;
        if (!assets_root.empty())
            removed_meta = m_metaStore->removeOne(source_vpath, assets_root);

        if (existing)
        {
            const std::string cooked_path = existing->cooked_path;
            const AssetID id = existing->id;
            registry->unregisterAsset(id);

            if (!cooked_path.empty())
            {
                auto cooked_physical = vfs->resolve(cooked_path);
                if (cooked_physical)
                {
                    std::error_code ec;
                    std::filesystem::remove(*cooked_physical, ec);
                }
            }
        }

        if (removed_meta || existing)
        {
            HBD_CORE_INFO("{} remove_completed source_path={} removed_meta={} had_registry_entry={}",
                          kEditorResourceLogTag,
                          source_vpath,
                          removed_meta ? "true" : "false",
                          existing ? "true" : "false");
        }

        return removed_meta || existing != nullptr;
    }

    bool EditorResourceSystem::normalizeAssetLogicalPath(const std::string& input, std::string& out_path)
    {
        const auto colon_pos = input.find(':');
        if (colon_pos == std::string::npos || colon_pos == 0 || colon_pos + 1 >= input.size())
            return false;

        const bool is_windows_drive =
            (colon_pos == 1 && std::isalpha(static_cast<unsigned char>(input[0])) != 0);
        if (is_windows_drive)
            return false;

        std::string alias = toLower(input.substr(0, colon_pos));
        std::string rel = input.substr(colon_pos + 1);
        if (alias != "asset")
            return false;

        if (rel.empty() || rel.front() == '/' || rel.front() == '\\')
            return false;

        for (char& ch : rel)
        {
            if (ch == '\\')
                ch = '/';
        }
        if (rel.find(':') != std::string::npos)
            return false;

        std::filesystem::path rel_path(rel);
        if (rel_path.is_absolute())
            return false;

        rel_path = rel_path.lexically_normal();
        if (hasParentTraversal(rel_path))
            return false;

        std::string norm_rel = rel_path.generic_string();
        while (norm_rel.rfind("./", 0) == 0)
            norm_rel.erase(0, 2);

        if (norm_rel.empty() || norm_rel == ".")
            return false;

        out_path = alias + ":" + norm_rel;
        return true;
    }

    AssetSourceChangeType EditorResourceSystem::mergeChangeType(AssetSourceChangeType existing,
                                                                AssetSourceChangeType incoming)
    {
        if (existing == AssetSourceChangeType::Removed || incoming == AssetSourceChangeType::Removed)
            return AssetSourceChangeType::Removed;

        if (existing == AssetSourceChangeType::Added || incoming == AssetSourceChangeType::Added)
            return AssetSourceChangeType::Added;

        return AssetSourceChangeType::Modified;
    }

    std::string EditorResourceSystem::buildDefaultCookedPath(const AssetMetadata& meta)
    {
        if (meta.source_path.empty())
            return {};

        const auto colon_pos = meta.source_path.find(':');
        if (colon_pos == std::string::npos || colon_pos + 1 >= meta.source_path.size())
            return {};

        std::filesystem::path rel(meta.source_path.substr(colon_pos + 1));
        switch (meta.type)
        {
        case AssetType::Mesh:
            rel.replace_extension(".hmesh");
            return std::string("cache:Cooked/") + rel.generic_string();
        case AssetType::Texture2D:
        case AssetType::TextureCube:
            rel.replace_extension(".htex");
            return std::string("cache:Cooked/") + rel.generic_string();
        case AssetType::Scene:
            rel.replace_extension(".hscene");
            return std::string("cache:Cooked/") + rel.generic_string();
        default:
            return {};
        }
    }

    std::string EditorResourceSystem::makeSimpleHash(const std::filesystem::path& file)
    {
        std::error_code ec;

        auto size = std::filesystem::file_size(file, ec);
        if (ec)
            size = 0;

        ec.clear();
        auto last_write_time = std::filesystem::last_write_time(file, ec);
        const auto ticks = ec ? 0LL : static_cast<long long>(last_write_time.time_since_epoch().count());

        return std::to_string(static_cast<unsigned long long>(size)) + "_" + std::to_string(ticks);
    }
} // namespace Hybrid


