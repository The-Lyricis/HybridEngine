#include "asset_manager.h"

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    AssetManager::AssetManager(std::shared_ptr<IVirtualFileSystem> vfs, std::shared_ptr<AssetRegistry> registry)
        : m_vfs(std::move(vfs)), m_registry(std::move(registry))
    {
    }

    AssetState AssetManager::getState(AssetID id) const
    {
        std::scoped_lock lock(m_mutex);
        auto it = m_state.find(id);
        return it == m_state.end() ? AssetState::Unloaded : it->second;
    }

    void AssetManager::unload(AssetID id)
    {
        std::scoped_lock lock(m_mutex);
        m_cache.erase(id);
        m_inFlight.erase(id);
        m_state[id] = AssetState::Unloaded;
    }

    std::shared_future<std::shared_ptr<void>> AssetManager::loadInternalAsync(std::type_index ti, AssetType type, AssetID id)
    {
        // 先检查缓存 / in-flight
        {
            std::scoped_lock lock(m_mutex);
            auto it_cache = m_cache.find(id);
            if (it_cache != m_cache.end())
            {
                std::promise<std::shared_ptr<void>> ready;
                ready.set_value(it_cache->second);
                return ready.get_future().share();
            }

            auto it = m_inFlight.find(id);
            if (it != m_inFlight.end())
                return it->second;
        }

        // 创建 promise/future，记录 in-flight
        auto promise_ptr = std::make_shared<std::promise<std::shared_ptr<void>>>();
        auto fut = promise_ptr->get_future().share();
        {
            std::scoped_lock lock(m_mutex);
            m_inFlight[id] = fut;
            m_state[id] = AssetState::Loading;
        }

        // 启动后台线程执行加载
        std::thread([this, promise_ptr, ti, type, id]()
                    {
            auto result = loadInternal(ti, type, id);
            promise_ptr->set_value(result); })
            .detach();

        return fut;
    }

    std::shared_ptr<void> AssetManager::performLoad(const AssetMetadata &meta, std::type_index ti, AssetType type)
    {
        LoaderKey key{ti, type};
        std::function<std::shared_ptr<void>(const AssetMetadata &, IVirtualFileSystem &)> loader;
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
        catch (const std::exception &e)
        {
            HBD_CORE_ERROR("Asset load exception: {}", e.what());
            return nullptr;
        }
    }

    std::shared_ptr<void> AssetManager::loadInternal(std::type_index ti, AssetType type, AssetID id)
    {
        std::shared_future<std::shared_ptr<void>> wait_future;

        // 1) 缓存 / in-flight 复用（取副本，出锁等待）
        {
            std::scoped_lock lock(m_mutex);
            if (auto it_cache = m_cache.find(id); it_cache != m_cache.end())
                return it_cache->second;

            if (auto it_f = m_inFlight.find(id); it_f != m_inFlight.end())
                wait_future = it_f->second;

            if (!wait_future.valid())
                m_state[id] = AssetState::Loading;
        }

        if (wait_future.valid())
            return wait_future.get(); // 出锁等待，避免死锁

        // 2) 拷贝元数据（防止热重载期间指针失效）
        AssetMetadata meta_copy;
        {
            const AssetMetadata *meta = m_registry ? m_registry->find(id) : nullptr;
            if (!meta)
            {
                std::scoped_lock lock(m_mutex);
                m_state[id] = AssetState::Failed;
                return nullptr;
            }
            meta_copy = *meta;
        }

        // 3) 执行加载
        auto instance = performLoad(meta_copy, ti, type);

        // 4) 写回状态 / 缓存，清理 in-flight
        {
            std::scoped_lock lock(m_mutex);
            if (instance)
            {
                m_cache[id] = instance;
                m_state[id] = AssetState::Loaded;
            }
            else
            {
                m_state[id] = AssetState::Failed;
            }
            m_inFlight.erase(id);
        }

        return instance;
    }

} // namespace Hybrid
