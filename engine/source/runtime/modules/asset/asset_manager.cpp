#include "asset_manager.h"

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kAssetManagerLogTag = "[AssetManager]";
    }

    AssetManager::AssetManager(std::shared_ptr<IVirtualFileSystem> vfs,
                               std::shared_ptr<AssetRegistry> registry,
                               std::shared_ptr<JobSystem> job_system)
        : m_vfs(std::move(vfs)), m_registry(std::move(registry)), m_jobSystem(std::move(job_system))
    {
    }

    AssetManager::~AssetManager()
    {
        shutdown();
    }

    void AssetManager::shutdown()
    {
        {
            std::scoped_lock lock(m_mutex);
            if (m_shutdown)
                return;
            m_shutdown = true;
            for (auto& [id, generation] : m_generation)
                ++generation;
        }

        if (m_jobSystem)
            m_jobSystem->waitIdle();

        std::scoped_lock lock(m_mutex);
        m_inFlight.clear();
        m_cache.clear();
        m_state.clear();
        m_loaders.clear();
        m_default.clear();
        m_generation.clear();
    }

    AssetState AssetManager::getState(AssetID id) const
    {
        std::scoped_lock lock(m_mutex);
        if (m_shutdown)
            return AssetState::Unloaded;
        auto it = m_state.find(id);
        return it == m_state.end() ? AssetState::Unloaded : it->second;
    }

    void AssetManager::unload(AssetID id)
    {
        std::scoped_lock lock(m_mutex);
        ++m_generation[id];
        m_cache.erase(id);
        m_inFlight.erase(id);
        m_state[id] = AssetState::Unloaded;
    }

    std::shared_future<std::shared_ptr<void>>
    AssetManager::loadInternalAsync(std::type_index ti, AssetType type, AssetID id)
    {
        auto promise = std::make_shared<std::promise<std::shared_ptr<void>>>();
        auto future = promise->get_future().share();
        uint64_t generation = 0;

        {
            std::scoped_lock lock(m_mutex);
            if (m_shutdown)
            {
                promise->set_value(nullptr);
                return future;
            }
            if (auto cached = m_cache.find(id); cached != m_cache.end())
            {
                promise->set_value(cached->second);
                return future;
            }
            if (auto running = m_inFlight.find(id); running != m_inFlight.end())
                return running->second.future;

            generation = m_generation[id];
            m_inFlight[id] = InFlightEntry{generation, future};
            m_state[id] = AssetState::Loading;
        }

        auto self = shared_from_this();
        auto task = [self, promise, ti, type, id, generation]() mutable
        {
            std::shared_ptr<void> instance;
            try
            {
                auto meta = self->m_registry ? self->m_registry->find(id) : std::nullopt;
                if (meta && meta->is_valid)
                    instance = self->performLoad(*meta, ti, type);
            }
            catch (const std::exception& error)
            {
                HBD_CORE_ERROR("{} async_load_exception asset_id={} reason={}",
                               kAssetManagerLogTag, id.value, error.what());
            }
            catch (...)
            {
                HBD_CORE_ERROR("{} async_load_exception asset_id={} reason=unknown",
                               kAssetManagerLogTag, id.value);
            }

            {
                std::scoped_lock lock(self->m_mutex);
                const auto generation_it = self->m_generation.find(id);
                const bool current = !self->m_shutdown &&
                                     generation_it != self->m_generation.end() &&
                                     generation_it->second == generation;
                if (current && instance)
                {
                    self->m_cache[id] = instance;
                    self->m_state[id] = AssetState::Loaded;
                }
                else if (current)
                {
                    self->m_state[id] = AssetState::Failed;
                }

                auto running = self->m_inFlight.find(id);
                if (running != self->m_inFlight.end() && running->second.generation == generation)
                    self->m_inFlight.erase(running);
                if (!current)
                    instance.reset();
            }
            promise->set_value(instance);
        };

        if (m_jobSystem && m_jobSystem->isRunning())
        {
            try
            {
                (void)m_jobSystem->submit(task);
            }
            catch (...)
            {
                task();
            }
        }
        else
        {
            task();
        }
        return future;
    }

    std::shared_ptr<void> AssetManager::performLoad(const AssetMetadata& meta, std::type_index ti, AssetType type)
    {
        LoaderKey key{ti, type};
        std::function<std::shared_ptr<void>(const AssetMetadata&, IVirtualFileSystem&)> loader;
        {
            std::scoped_lock lock(m_mutex);
            auto it = m_loaders.find(key);
            if (it != m_loaders.end())
                loader = it->second;
        }
        if (!loader)
            return nullptr;

        try
        {
            if (!m_vfs)
                throw std::runtime_error("VFS is null");
            return loader(meta, *m_vfs);
        }
        catch (const std::exception& error)
        {
            HBD_CORE_ERROR("{} load_exception asset_id={} asset_type={} reason={}",
                           kAssetManagerLogTag, meta.id.value, static_cast<uint32_t>(type), error.what());
            return nullptr;
        }
    }

    std::shared_ptr<void> AssetManager::loadInternal(std::type_index ti, AssetType type, AssetID id)
    {
        auto instance = loadInternalAsync(ti, type, id).get();
        if (instance)
            return instance;

        auto fallback = getDefaultByTypeIndex(ti);
        if (fallback)
            HBD_CORE_WARN("{} load_fallback_selected asset_id={} reason=load_failed", kAssetManagerLogTag, id.value);
        return fallback;
    }
} // namespace Hybrid
