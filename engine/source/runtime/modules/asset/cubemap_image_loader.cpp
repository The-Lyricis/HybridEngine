#include "cubemap_image_loader.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>
#include <stb_image.h>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;
        constexpr const char* kCubemapImageLoaderLogTag = "[CubemapImageLoader]";

        constexpr std::array<const char*, 6> kFaceKeys = {
            "px",
            "nx",
            "py",
            "ny",
            "pz",
            "nz",
        };

        bool isLogicalAssetPath(const std::string& path)
        {
            return path.rfind("asset:", 0) == 0 ||
                   path.rfind("cache:", 0) == 0 ||
                   path.rfind("project:", 0) == 0 ||
                   path.rfind("build:", 0) == 0 ||
                   path.rfind("engine:", 0) == 0;
        }

        std::string resolveFacePath(const AssetMetadata& meta, const std::string& raw_path)
        {
            if (raw_path.empty())
                return {};
            if (isLogicalAssetPath(raw_path))
                return raw_path;

            std::filesystem::path base_path(meta.source_path);
            const std::filesystem::path parent = base_path.has_parent_path() ? base_path.parent_path() : std::filesystem::path{};
            return (parent / raw_path).lexically_normal().generic_string();
        }
    } // namespace

    std::vector<char> CubemapImageLoader::readBytes(const IVirtualFileSystem& vfs, const std::string& path) const
    {
        if (path.empty())
            return {};
        return vfs.readAll(path);
    }

    std::shared_ptr<CubemapImageData> CubemapImageLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        const std::string source_path = meta.source_path;
        if (source_path.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} reason=empty_source_path",
                           kCubemapImageLoaderLogTag,
                           meta.id.value);
            return nullptr;
        }

        const std::vector<char> bytes = readBytes(vfs, source_path);
        if (bytes.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=file_not_found",
                           kCubemapImageLoaderLogTag,
                           meta.id.value,
                           source_path);
            return nullptr;
        }

        json root = json::parse(bytes.begin(), bytes.end(), nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=invalid_json",
                           kCubemapImageLoaderLogTag,
                           meta.id.value,
                           source_path);
            return nullptr;
        }

        auto cubemap = std::make_shared<CubemapImageData>();
        if (root.contains("srgb") && root["srgb"].is_boolean())
            cubemap->srgb = root["srgb"].get<bool>();
        if (root.contains("generate_mips") && root["generate_mips"].is_boolean())
            cubemap->generate_mips = root["generate_mips"].get<bool>();

        for (size_t face_index = 0; face_index < kFaceKeys.size(); ++face_index)
        {
            const char* key = kFaceKeys[face_index];
            if (!root.contains(key) || !root[key].is_string())
            {
                HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} face={} reason=missing_face_path",
                               kCubemapImageLoaderLogTag,
                               meta.id.value,
                               source_path,
                               key);
                return nullptr;
            }

            const std::string resolved_face_path = resolveFacePath(meta, root[key].get<std::string>());
            const std::vector<char> face_bytes = readBytes(vfs, resolved_face_path);
            if (face_bytes.empty())
            {
                HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} face={} path={} reason=face_file_not_found",
                               kCubemapImageLoaderLogTag,
                               meta.id.value,
                               source_path,
                               key,
                               resolved_face_path);
                return nullptr;
            }

            int width = 0;
            int height = 0;
            int comp = 0;
            stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(face_bytes.data()),
                                                    static_cast<int>(face_bytes.size()),
                                                    &width,
                                                    &height,
                                                    &comp,
                                                    4);
            if (!pixels || width <= 0 || height <= 0)
            {
                HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} face={} path={} reason=face_decode_failed",
                               kCubemapImageLoaderLogTag,
                               meta.id.value,
                               source_path,
                               key,
                               resolved_face_path);
                if (pixels)
                    stbi_image_free(pixels);
                return nullptr;
            }

            if (cubemap->width == 0 && cubemap->height == 0)
            {
                cubemap->width = static_cast<uint32_t>(width);
                cubemap->height = static_cast<uint32_t>(height);
                cubemap->format = TextureFormat::RGBA8;
            }
            else if (cubemap->width != static_cast<uint32_t>(width) ||
                     cubemap->height != static_cast<uint32_t>(height))
            {
                HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} face={} reason=face_size_mismatch expected={}x{} actual={}x{}",
                               kCubemapImageLoaderLogTag,
                               meta.id.value,
                               source_path,
                               key,
                               cubemap->width,
                               cubemap->height,
                               width,
                               height);
                stbi_image_free(pixels);
                return nullptr;
            }

            const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            cubemap->faces[face_index].pixels.resize(pixel_count);
            std::memcpy(cubemap->faces[face_index].pixels.data(), pixels, pixel_count);
            stbi_image_free(pixels);
        }

        return cubemap->isValid() ? cubemap : nullptr;
    }
} // namespace Hybrid
