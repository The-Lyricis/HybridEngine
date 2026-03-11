#include "audio_importer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kAudioImporterLogTag = "[AudioImporter]";

        std::string toLower(std::string v)
        {
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return v;
        }

        bool isAudioExt(std::string_view ext)
        {
            const std::string e = toLower(std::string(ext));
            return e == ".wav" || e == ".ogg";
        }
    } // namespace

    bool AudioImporter::supportsExtension(std::string_view ext) const
    {
        return isAudioExt(ext);
    }

    ImportResult AudioImporter::importAsset(const ImportRequest& request,
                                            AssetRegistry& registry,
                                            IVirtualFileSystem& vfs)
    {
        (void)vfs;
        ImportResult out{};
        HBD_CORE_INFO("{} import_started source_path={} requested_cooked_path={}",
                      kAudioImporterLogTag,
                      request.source_path,
                      request.cooked_path.empty() ? "<default>" : request.cooked_path);

        const auto pos = request.source_path.find(':');
        if (pos == std::string::npos || pos + 1 >= request.source_path.size())
        {
            out.message = "AudioImporter: source_path must be alias:relative";
            return out;
        }

        const std::string rel = request.source_path.substr(pos + 1);
        const std::string ext = toLower(std::filesystem::path(rel).extension().string());
        if (!isAudioExt(ext))
        {
            out.message = "AudioImporter: unsupported extension";
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

        meta.type = AssetType::Audio;
        meta.source_path = request.source_path;
        meta.cooked_path = request.cooked_path;
        meta.hash = request.hash;
        meta.is_valid = true;

        out.success = true;
        out.primary_id = meta.id;
        out.assets.push_back(std::move(meta));
        HBD_CORE_INFO("{} import_completed source_path={} cooked_path={} asset_id={}",
                      kAudioImporterLogTag,
                      request.source_path,
                      meta.cooked_path.empty() ? "<none>" : meta.cooked_path,
                      out.primary_id.value);
        return out;
    }
} // namespace Hybrid

