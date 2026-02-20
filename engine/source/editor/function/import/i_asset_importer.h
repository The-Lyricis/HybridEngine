#pragma once

#include <string_view>

#include "import_types.h"
#include "runtime/function/asset/asset_registry.h"

namespace Hybrid
{
    class IAssetImporter
    {
    public:
        virtual ~IAssetImporter() = default;

        // Primary type produced by this importer.
        virtual AssetType primaryType() const = 0;

        // Extension routing (e.g. ".png", ".fbx").
        virtual bool supportsExtension(std::string_view ext) const = 0;

        // Build metadata (and later cooked outputs) for requested asset.
        virtual ImportResult importAsset(const ImportRequest& request, AssetRegistry& registry) = 0;
    };
} // namespace Hybrid

