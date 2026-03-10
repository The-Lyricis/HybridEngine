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

    AssetType AssetTypeFromString(const std::string& name);
    std::string AssetTypeToString(AssetType type);

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
        AssetID id{};
        uint32_t generation = 0;

        bool valid() const { return id.value != 0; }
    };

    struct AssetMetadata
    {
        AssetID id{};
        AssetType type{AssetType::Unknown};
        std::string source_path;
        std::string cooked_path;
        std::string hash;
        AssetID parent_id{};
        std::string subasset_key;
        std::vector<AssetID> hard_deps;
        std::vector<AssetID> soft_deps;
        bool is_valid = false;
    };
} // namespace Hybrid
