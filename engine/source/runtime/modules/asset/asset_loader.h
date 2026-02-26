#pragma once

#include <memory>

#include "asset_type.h"
#include "asset_registry.h"
#include "runtime/core/base/vfs/virtual_file_system.h"

namespace Hybrid
{
    // 杩愯鏃惰祫婧愬姞杞藉櫒鎺ュ彛锛堟寜璧勬簮绫诲瀷鍒嗗彂锛?
    template <typename T> class IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;

        // 澹版槑璇?Loader 瀵瑰簲鐨勮祫婧愮被鍨?
        virtual AssetType assetType() const = 0;

        // 浠庡厓鏁版嵁 + VFS 鏋勫缓杩愯鏃跺璞?
        virtual std::shared_ptr<T> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) = 0;
    };
} // namespace Hybrid


