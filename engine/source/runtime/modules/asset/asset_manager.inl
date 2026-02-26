#pragma once

namespace Hybrid
{
    template <typename T> void AssetManager::registerLoader(AssetType type, AssetLoadFunc<T> fn)
    {
        LoaderKey key{typeid(T), type};
        std::scoped_lock lock(m_mutex);
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
        const AssetMetadata* meta = m_registry ? m_registry->find(id) : nullptr;
        if (!meta || !meta->is_valid)
            return nullptr;

        auto ptr = loadInternal(typeid(T), meta->type, id);
        return std::static_pointer_cast<T>(ptr);
    }

    template <typename T> AssetFuture<T> AssetManager::loadAsync(AssetID id)
    {
        const AssetMetadata* meta = m_registry ? m_registry->find(id) : nullptr;
        if (!meta || !meta->is_valid)
            return AssetFuture<T>();

        auto fut = loadInternalAsync(typeid(T), meta->type, id);
        return AssetFuture<T>(std::move(fut));
    }

    template <typename T> void AssetManager::setDefault(const std::shared_ptr<T>& def)
    {
        std::scoped_lock lock(m_mutex);
        m_default[typeid(T)] = def;
    }

    template <typename T> std::shared_ptr<T> AssetManager::getDefault() const
    {
        std::scoped_lock lock(m_mutex);
        auto it = m_default.find(typeid(T));
        if (it != m_default.end())
            return std::static_pointer_cast<T>(it->second);
        return nullptr;
    }
} // namespace Hybrid
