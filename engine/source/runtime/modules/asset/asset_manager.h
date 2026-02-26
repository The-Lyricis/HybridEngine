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
#include "runtime/core/base/vfs/virtual_file_system.h"

namespace Hybrid
{
    enum class AssetState
    {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };

    // 杞婚噺鍖呰锛屽皝瑁?shared_future<void> -> shared_ptr<T>
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

    // 杩愯鏃?Loader锛氫粠 cooked/婧愭枃浠舵瀯寤?T
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

        // 璁剧疆/鑾峰彇榛樿璧勬簮锛圠oader 澶辫触鎴栧紓姝ラ檺鍒舵椂鍙洖閫€锛?
        template <typename T> void setDefault(const std::shared_ptr<T>& def);
        template <typename T> std::shared_ptr<T> getDefault() const;

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

        // 鎵ц瀹為檯鍔犺浇锛屼笉瑙︾缂撳瓨/鐘舵€侊紱闇€鍦ㄥ閮ㄨ皟鐢ㄥ璁剧疆鐘舵€?
        std::shared_ptr<void> performLoad(const AssetMetadata& meta, std::type_index ti, AssetType type);

        // 榛樿璧勬簮鏌ユ壘锛堣皟鐢ㄦ柟闇€鑷鍔犻攣鎴栧湪閿佸璋冪敤灏佽鍑芥暟锛?
        std::shared_ptr<void> getDefaultByTypeIndex(std::type_index ti) const
        {
            std::scoped_lock lock(m_mutex);
            auto it = m_default.find(ti);
            return it == m_default.end() ? nullptr : it->second;
        }

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

        std::unordered_map<std::type_index, std::shared_ptr<void>> m_default;
    };
} // namespace Hybrid

#include "asset_manager.inl"

