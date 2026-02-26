#pragma once

#include <cstdint>
#include <string>

namespace Hybrid
{
    enum class AssetSourceEventType : uint8_t
    {
        Added = 0,
        Modified,
        Removed,
        Moved
    };

    struct AssetSourceEvent
    {
        AssetSourceEventType type = AssetSourceEventType::Modified;
        std::string path;
        std::string old_path;
        std::string new_path;
    };

    using EditorEntityID = uint32_t;
} // namespace Hybrid
