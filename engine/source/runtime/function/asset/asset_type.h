#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Hybrid
{
    enum class AssetType : uint8_t
    {
        Unknown = 0,
        Texture2D,
        TextureCube,
        Mesh,
        Material,
        Shader,
        Scene,
        Audio,
        Animation,
        Script
    };

    // 序列化映射
    AssetType    AssetTypeFromString(const std::string& name);
    std::string  AssetTypeToString(AssetType type);

    struct AssetID
    {
        uint64_t value = 0;

        static AssetID FromRaw(uint64_t v) { return AssetID{v}; }

        bool operator==(const AssetID& rhs) const { return value == rhs.value; }
        bool operator!=(const AssetID& rhs) const { return value != rhs.value; }

        struct Hasher
        {
            size_t operator()(const AssetID& id) const { return std::hash<uint64_t>()(id.value); }
        };
    };

    struct AssetHandle
    {
        AssetID  id{};
        uint32_t generation = 0;

        bool valid() const { return id.value != 0; }
    };

    struct AssetMetadata
    {
        AssetID     id{};
        AssetType   type{AssetType::Unknown}; // 资源类型
        std::string source_path;              // 逻辑路径 alias:/relative
        std::string cooked_path;              // 逻辑路径 alias:/relative
        std::string hash;                     // 源文件 + 导入设置哈希
        std::vector<AssetID> hard_deps;       // 必需依赖
        std::vector<AssetID> soft_deps;       // 可选依赖
        bool is_valid = false;
    };
} // namespace Hybrid

