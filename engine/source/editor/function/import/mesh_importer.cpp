#include "mesh_importer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Hybrid
{
    namespace
    {
        std::string toLower(std::string v)
        {
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return v;
        }

        bool isMeshExt(std::string_view ext)
        {
            const std::string e = toLower(std::string(ext));
            return e == ".fbx" || e == ".gltf" || e == ".glb" || e == ".obj";
        }
    } // namespace

    bool MeshImporter::supportsExtension(std::string_view ext) const
    {
        return isMeshExt(ext);
    }

    ImportResult MeshImporter::importAsset(const ImportRequest& request, AssetRegistry& registry)
    {
        ImportResult out{};

        const auto pos = request.source_path.find(':');
        if (pos == std::string::npos || pos + 1 >= request.source_path.size())
        {
            out.message = "MeshImporter: source_path must be alias:relative";
            return out;
        }

        const std::string rel = request.source_path.substr(pos + 1);
        const std::string ext = toLower(std::filesystem::path(rel).extension().string());
        if (!isMeshExt(ext))
        {
            out.message = "MeshImporter: unsupported extension";
            return out;
        }

        AssetMetadata meta{};
        if (const auto* existing = registry.findByPath(request.source_path))
        {
            meta = *existing;
        }
        else
        {
            meta.id = registry.generateUniqueID();
        }

        meta.type = AssetType::Mesh;
        meta.source_path = request.source_path;
        meta.cooked_path = request.cooked_path;
        meta.hash = request.hash;
        meta.is_valid = true;

        out.success = true;
        out.primary_id = meta.id;
        out.assets.push_back(std::move(meta));
        return out;
    }
} // namespace Hybrid

