#pragma once

#include <memory>

#include "asset_type.h"
#include "asset_registry.h"
#include "runtime/core/base/virtual_file_system.h"

namespace Hybrid
{
    // 运行时资源加载器接口（按资源类型分发）
    template <typename T> class IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;

        // 声明该 Loader 对应的资源类型
        virtual AssetType assetType() const = 0;

        // 从元数据 + VFS 构建运行时对象
        virtual std::shared_ptr<T> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) = 0;
    };
} // namespace Hybrid

