#include "texture_importer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include <stb_image.h>

#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/texture_cooked_format.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kTextureImporterLogTag = "[TextureImporter]";

        std::string toLower(std::string v)
        {
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return v;
        }

        bool isTextureExt(std::string_view ext)
        {
            const std::string e = toLower(std::string(ext));
            return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".hdr";
        }

        bool splitLogicalPath(const std::string& path, std::string& alias_out, std::string& rel_out)
        {
            const auto pos = path.find(':');
            if (pos == std::string::npos || pos == 0 || pos + 1 >= path.size())
                return false;

            alias_out = path.substr(0, pos);
            rel_out = path.substr(pos + 1);
            if (rel_out.empty() || rel_out.front() == '/' || rel_out.front() == '\\')
                return false;
            std::replace(rel_out.begin(), rel_out.end(), '\\', '/');
            return true;
        }

        std::string buildDefaultCookedPath(const std::string& source_path)
        {
            std::string alias, rel;
            if (!splitLogicalPath(source_path, alias, rel))
                return {};

            std::filesystem::path p(rel);
            p.replace_extension(".htex");
            return std::string("cache:Cooked/") + p.generic_string();
        }
    } // namespace

    bool TextureImporter::supportsExtension(std::string_view ext) const
    {
        return isTextureExt(ext);
    }

    ImportResult TextureImporter::importAsset(const ImportRequest& request,
                                              AssetRegistry& registry,
                                              IVirtualFileSystem& vfs)
    {
        ImportResult out{};
        HBD_CORE_INFO("{} import_started source_path={} requested_cooked_path={}",
                      kTextureImporterLogTag,
                      request.source_path,
                      request.cooked_path.empty() ? "<default>" : request.cooked_path);

        std::string src_alias, src_rel;
        if (!splitLogicalPath(request.source_path, src_alias, src_rel))
        {
            out.message = "TextureImporter: source_path must be alias:relative";
            return out;
        }

        const std::string ext = toLower(std::filesystem::path(src_rel).extension().string());
        if (!isTextureExt(ext))
        {
            out.message = "TextureImporter: unsupported extension";
            return out;
        }

        std::vector<char> source_bytes = vfs.readAll(request.source_path);
        if (source_bytes.empty())
        {
            out.message = "TextureImporter: source file not found";
            return out;
        }

        int w = 0, h = 0, comp = 0;
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(source_bytes.data()),
                                                static_cast<int>(source_bytes.size()),
                                                &w,
                                                &h,
                                                &comp,
                                                4);
        if (!pixels || w <= 0 || h <= 0)
        {
            if (pixels)
                stbi_image_free(pixels);
            out.message = "TextureImporter: decode source failed";
            return out;
        }

        const std::string cooked_path = request.cooked_path.empty() ? buildDefaultCookedPath(request.source_path)
                                                                    : request.cooked_path;
        auto cooked_native =vfs.resolveForWrite(cooked_path);
        if (!cooked_native)
        {
            stbi_image_free(pixels);
            out.message = "TextureImporter: cannot resolve cooked path";
            return out;
        }

        std::vector<char> cooked_bytes;
        if (!HtexEncodeRgba8(static_cast<uint32_t>(w),
                             static_cast<uint32_t>(h),
                             reinterpret_cast<const uint8_t*>(pixels),
                             HTEX_FLAG_NONE,
                             cooked_bytes))
        {
            stbi_image_free(pixels);
            out.message = "TextureImporter: encode cooked failed";
            return out;
        }
        stbi_image_free(pixels);

        std::error_code ec;
        std::filesystem::create_directories(cooked_native->parent_path(), ec);
        if (ec)
        {
            out.message = "TextureImporter: create cooked directory failed";
            return out;
        }

        {
            std::ofstream ofs(*cooked_native, std::ios::binary | std::ios::trunc);
            if (!ofs)
            {
                out.message = "TextureImporter: open cooked file failed";
                return out;
            }
            ofs.write(cooked_bytes.data(), static_cast<std::streamsize>(cooked_bytes.size()));
            if (!ofs.good())
            {
                out.message = "TextureImporter: write cooked file failed";
                return out;
            }
        }

        AssetMetadata meta{};
        if (const auto existing = registry.findByPath(request.source_path))
            meta = *existing;
        else
            meta.id = registry.generateUniqueID();

        meta.type = AssetType::Texture2D;
        meta.source_path = request.source_path;
        meta.cooked_path = cooked_path;
        meta.hash = request.hash;
        meta.is_valid = true;

        out.success = true;
        out.primary_id = meta.id;
        out.assets.push_back(std::move(meta));
        HBD_CORE_INFO("{} import_completed source_path={} cooked_path={} asset_id={} size={}x{}",
                      kTextureImporterLogTag,
                      request.source_path,
                      cooked_path,
                      out.primary_id.value,
                      w,
                      h);
        return out;
    }
} // namespace Hybrid

