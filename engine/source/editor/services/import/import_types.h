#pragma once

#include <string>
#include <vector>

#include "runtime/modules/asset/asset_type.h"

namespace Hybrid
{
    struct ImportRequest
    {
        // Strict logical path: alias:relative
        std::string source_path;
        // Optional cooked output path (alias:relative).
        std::string cooked_path;
        // Optional content/settings hash.
        std::string hash;

        // Optional routing hint.
        AssetType preferred_type = AssetType::Unknown;
        bool force_reimport = false;
    };

    struct ImportResult
    {
        bool success = false;
        AssetID primary_id{};
        std::string message;

        // Main + sub-assets (future-proof for FBX/GLTF split).
        std::vector<AssetMetadata> assets;
    };

    struct ImportPreparedResult
    {
        ImportRequest request;
        ImportResult result;
    };

    enum class ImportTaskState : uint8_t { Queued = 0, Running, Succeeded, Failed };

    struct ImportTaskSnapshot
    {
        uint64_t id = 0;
        ImportTaskState state = ImportTaskState::Queued;
        std::string source_path;
        std::string stage;
        uint64_t elapsed_ms = 0;
        std::string message;
    };
} // namespace Hybrid


