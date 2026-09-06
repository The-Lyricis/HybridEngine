#pragma once

namespace Hybrid
{
    template <typename T> void AssetManager::registerLoader(AssetType type, AssetLoadFunc<T> fn)
    {
        LoaderKey key{typeid(T), type};
        std::scoped_lock lock(m_mutex);
        if (m_shutdown)
            return;
        m_loaders[key] = [fn](const AssetMetadata& meta, IVirtualFileSystem& vfs) {
            return std::static_pointer_cast<void>(fn(meta, vfs));
        };
    }

    template <typename T> void AssetManager::registerLoader(const std::shared_ptr<IAssetLoader<T>>& loader)
    {
        if (!loader)
            return;
        registerLoader<T>(loader->assetType(), [loader](const AssetMetadata& meta, IVirtualFileSystem& vfs) {
            return loader->load(meta, vfs);
        });
    }

    template <typename T> std::shared_ptr<T> AssetManager::loadSync(AssetID id)
    {
        auto meta = m_registry ? m_registry->find(id) : std::nullopt;
        if (!meta || !meta->is_valid)
            return nullptr;
        return std::static_pointer_cast<T>(loadInternal(typeid(T), meta->type, id));
    }

    template <typename T> AssetFuture<T> AssetManager::loadAsync(AssetID id)
    {
        auto meta = m_registry ? m_registry->find(id) : std::nullopt;
        if (!meta || !meta->is_valid)
            return AssetFuture<T>();
        return AssetFuture<T>(loadInternalAsync(typeid(T), meta->type, id));
    }

    template <typename T> void AssetManager::registerResident(AssetID id, const std::shared_ptr<T>& asset)
    {
        if (id.value == 0 || !asset)
            return;
        std::scoped_lock lock(m_mutex);
        if (m_shutdown)
            return;
        ++m_generation[id];
        m_cache[id] = std::static_pointer_cast<void>(asset);
        m_state[id] = AssetState::Loaded;
        m_inFlight.erase(id);
    }

    template <typename T> void AssetManager::setDefault(const std::shared_ptr<T>& def)
    {
        std::scoped_lock lock(m_mutex);
        if (!m_shutdown)
            m_default[typeid(T)] = def;
    }

    template <typename T> std::shared_ptr<T> AssetManager::getDefault() const
    {
        std::scoped_lock lock(m_mutex);
        auto it = m_default.find(typeid(T));
        return it == m_default.end() ? nullptr : std::static_pointer_cast<T>(it->second);
    }
} // namespace Hybrid
