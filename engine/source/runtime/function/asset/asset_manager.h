#pragma once

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <typeindex>
#include <unordered_map>

#include "asset_type.h"
#include "asset_registry.h"
#include "asset_loader.h"
#include "runtime/core/base/virtual_file_system.h"

namespace Hybrid
{
    enum class AssetState
    {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };

    // 轻量包装，封装 shared_future<void> -> shared_ptr<T>
    template <typename T> class AssetFuture
    {
    public:
        AssetFuture() = default;
        explicit AssetFuture(std::shared_future<std::shared_ptr<void>> f) : m_future(std::move(f)) {}

        bool valid() const { return m_future.valid(); }

        std::shared_ptr<T> get() const
        {
            if (!m_future.valid())
                return nullptr;
            return std::static_pointer_cast<T>(m_future.get());
        }

    private:
        std::shared_future<std::shared_ptr<void>> m_future;
    };

    // 运行时 Loader：从 cooked/源文件构建 T
    template <typename T> using AssetLoadFunc = std::function<std::shared_ptr<T>(const AssetMetadata&, IVirtualFileSystem&)>;

    class AssetManager
    {
    public:
        AssetManager(std::shared_ptr<IVirtualFileSystem> vfs, std::shared_ptr<AssetRegistry> registry);

        template <typename T> void registerLoader(AssetType type, AssetLoadFunc<T> fn);
        template <typename T> void registerLoader(const std::shared_ptr<IAssetLoader<T>>& loader);

        template <typename T> std::shared_ptr<T> loadSync(AssetID id);

        template <typename T> AssetFuture<T> loadAsync(AssetID id);

        void unload(AssetID id);
        AssetState getState(AssetID id) const;

    private:
        struct LoaderKey
        {
            std::type_index ti;
            AssetType       assetType;
            bool operator==(const LoaderKey& o) const { return ti == o.ti && assetType == o.assetType; }
        };
        struct LoaderKeyHasher
        {
            size_t operator()(const LoaderKey& k) const
            {
                return std::hash<size_t>()(k.ti.hash_code()) ^ (static_cast<size_t>(k.assetType) << 1);
            }
        };

        std::shared_ptr<void> loadInternal(std::type_index ti, AssetType type, AssetID id);
        std::shared_future<std::shared_ptr<void>> loadInternalAsync(std::type_index ti, AssetType type, AssetID id);

        // 执行实际加载，不触碰缓存/状态；需在外部调用处设置状态
        std::shared_ptr<void> performLoad(const AssetMetadata& meta, std::type_index ti, AssetType type);

    private:
        std::shared_ptr<IVirtualFileSystem> m_vfs;
        std::shared_ptr<AssetRegistry>      m_registry;

        mutable std::mutex m_mutex;
        std::unordered_map<AssetID, AssetState, AssetID::Hasher> m_state;
        std::unordered_map<AssetID, std::shared_ptr<void>, AssetID::Hasher> m_cache;
        std::unordered_map<AssetID, std::shared_future<std::shared_ptr<void>>, AssetID::Hasher> m_inFlight;

        std::unordered_map<LoaderKey,
                           std::function<std::shared_ptr<void>(const AssetMetadata&, IVirtualFileSystem&)>,
                           LoaderKeyHasher>
            m_loaders;
    };
} // namespace Hybrid

#include "asset_manager.inl"
