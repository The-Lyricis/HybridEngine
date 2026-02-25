#include "editor_resource_system.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "runtime/core/base/macro.h"
#include "runtime/function/asset/asset_meta_store.h"
#include "runtime/function/asset/asset_registry.h"
#include "runtime/function/asset/runtime_resource_system.h"

#include "editor/function/import/audio_importer.h"
#include "editor/function/import/mesh_importer.h"
#include "editor/function/import/texture_importer.h"

namespace Hybrid
{
    namespace
    {
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

        bool isMetaLogicalPath(const std::string& logical_path)
        {
            const auto pos = logical_path.find(':');
            const std::string rel = (pos == std::string::npos) ? logical_path : logical_path.substr(pos + 1);
            return std::filesystem::path(rel).extension() == ".meta";
        }
    } // namespace

    bool EditorResourceSystem::initialize(RuntimeResourceSystem& runtime_system)
    {
        auto registry = runtime_system.getRegistry();
        if (!registry)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime registry is null");
            return false;
        }

        auto vfs = runtime_system.getVFS();
        if (!vfs)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime vfs is null");
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
        m_bootstrap_done = false;
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

    void EditorResourceSystem::enqueueSourceChanged(const std::string& source_vpath, AssetSourceChangeType change)
    {
        if (!m_importManager)
            return;

        std::string normalized;
        if (!normalizeAssetLogicalPath(source_vpath, normalized))
        {
            HBD_CORE_WARN("EditorResourceSystem: ignore invalid source path {}", source_vpath);
            return;
        }

        if (isMetaLogicalPath(normalized))
            return;

        if (!m_importManager->canImport(normalized))
            return;

        const auto now = std::chrono::steady_clock::now();
        auto it = m_pending_changes.find(normalized);
        if (it == m_pending_changes.end())
        {
            m_pending_changes.emplace(normalized, PendingSourceChange{change, now});
            m_event_queue.push_back(normalized);
            return;
        }

        it->second.type = mergeChangeType(it->second.type, change);
        it->second.last_event_time = now;
    }

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
            m_pending_changes.erase(pending_it);
            (void)processOneEvent(path, change);
            ++jobs;
        }
    }

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

        size_t queued_missing_meta = 0;
        size_t queued_missing_cooked = 0;
        size_t scanned = 0;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(assets_root, ec), end;
        if (ec)
        {
            HBD_CORE_WARN("EditorResourceSystem: bootstrap scan failed at {} ({})", assets_root.string(), ec.message());
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
                enqueueSourceChanged(source_vpath, AssetSourceChangeType::AddedOrModified);
            }
        }

        m_bootstrap_done = true;
        HBD_CORE_INFO("EditorResourceSystem bootstrap: scanned={}, queued_missing_meta={}, queued_missing_cooked={}",
                      scanned,
                      queued_missing_meta,
                      queued_missing_cooked);
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
            HBD_CORE_ERROR("Editor save meta failed: asset root is empty");
            return false;
        }

        registry->registerAsset(meta);

        const bool ok = m_metaStore->saveOne(meta, asset_root);
        if (!ok)
        {
            HBD_CORE_ERROR("Editor save meta failed for {}", meta.source_path);
        }
        return ok;
    }

    void EditorResourceSystem::registerDefaultImporters()
    {
        if (!m_importManager)
            return;

        m_importManager->registerImporter(std::make_shared<TextureImporter>());
        m_importManager->registerImporter(std::make_shared<MeshImporter>());
        m_importManager->registerImporter(std::make_shared<AudioImporter>());
    }

    bool EditorResourceSystem::processOneEvent(const std::string& source_vpath, AssetSourceChangeType change)
    {
        if (change == AssetSourceChangeType::Removed)
            return handleRemove(source_vpath);

        return handleUpsert(source_vpath);
    }

    bool EditorResourceSystem::handleUpsert(const std::string& source_vpath)
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
            return handleRemove(source_vpath);
        }

        auto source_physical = vfs->resolve(source_vpath);
        if (!source_physical)
            return false;

        const std::string new_hash = makeSimpleHash(*source_physical);
        const AssetMetadata* existing = registry->findByPath(source_vpath);
        const bool cooked_exists =
            existing && !existing->cooked_path.empty() && vfs->exists(existing->cooked_path);

        if (existing && existing->is_valid && existing->hash == new_hash && cooked_exists)
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
            HBD_CORE_ERROR("EditorResourceSystem: import failed for {} ({})", source_vpath, result.message);
            return false;
        }

        HBD_CORE_INFO("EditorResourceSystem: imported {}", source_vpath);
        return true;
    }

    bool EditorResourceSystem::handleRemove(const std::string& source_vpath)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        auto registry = m_runtime->getRegistry();
        auto vfs = m_runtime->getVFS();
        if (!registry || !vfs)
            return false;

        const auto* existing = registry->findByPath(source_vpath);
        const std::filesystem::path assets_root = registry->getRoot();

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
            HBD_CORE_INFO("EditorResourceSystem: removed asset {}", source_vpath);
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
        if (incoming == AssetSourceChangeType::Removed)
            return AssetSourceChangeType::Removed;

        if (existing == AssetSourceChangeType::Removed && incoming == AssetSourceChangeType::AddedOrModified)
            return AssetSourceChangeType::AddedOrModified;

        return AssetSourceChangeType::AddedOrModified;
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
