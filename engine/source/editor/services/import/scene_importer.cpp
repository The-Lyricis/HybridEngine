#include "scene_importer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kSceneImporterLogTag = "[SceneImporter]";

        std::string toLower(std::string v)
        {
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
                });
            return v;
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

        bool isSceneExt(std::string_view ext)
        {
            return toLower(std::string(ext)) == ".scene";
        }

        // 这里先采用“Cooked=规范化后的 JSON 文本”策略（最小可行）
        // 后续你可以把 .hscene 变为二进制 cooked 格式（更快更小）
        std::string buildDefaultCookedPath(const std::string& source_path)
        {
            std::string alias, rel;
            if (!splitLogicalPath(source_path, alias, rel))
                return {};

            std::filesystem::path p(rel);
            p.replace_extension(".hscene");
            return std::string("cache:Cooked/") + p.generic_string();
        }

        // 可选：做最基本校验，避免把垃圾文件当 scene 导入
        bool validateSceneJson(const std::vector<char>& bytes, std::string& err)
        {
            try
            {
                const auto j = nlohmann::json::parse(bytes.begin(), bytes.end());

                if (!j.contains("meta") || !j["meta"].is_object())
                {
                    err = "missing meta";
                    return false;
                }
                const int ver = j["meta"].value("version", 0);
                if (ver <= 0)
                {
                    err = "invalid meta.version";
                    return false;
                }
                if (!j.contains("entities") || !j["entities"].is_array())
                {
                    err = "missing entities array";
                    return false;
                }
                return true;
            }
            catch (const std::exception& e)
            {
                err = e.what();
                return false;
            }
        }
    } // namespace

    bool SceneImporter::supportsExtension(std::string_view ext) const
    {
        return isSceneExt(ext);
    }

    ImportResult SceneImporter::importAsset(const ImportRequest& request,
        AssetRegistry& registry,
        IVirtualFileSystem& vfs)
    {
        ImportResult out{};
        HBD_CORE_INFO("{} import_started source_path={} requested_cooked_path={}",
                      kSceneImporterLogTag,
                      request.source_path,
                      request.cooked_path.empty() ? "<default>" : request.cooked_path);

        std::string src_alias, src_rel;
        if (!splitLogicalPath(request.source_path, src_alias, src_rel))
        {
            out.message = "SceneImporter: source_path must be alias:relative";
            return out;
        }

        const std::string ext = toLower(std::filesystem::path(src_rel).extension().string());
        if (!isSceneExt(ext))
        {
            out.message = "SceneImporter: unsupported extension";
            return out;
        }

        // 读源文件（scene 源文件就是可读 JSON）
        std::vector<char> source_bytes = vfs.readAll(request.source_path);
        if (source_bytes.empty())
        {
            out.message = "SceneImporter: source file not found";
            return out;
        }

        // 基本校验（可关闭，但建议保留）
        std::string validate_err;
        if (!validateSceneJson(source_bytes, validate_err))
        {
            out.message = std::string("SceneImporter: invalid scene json: ") + validate_err;
            return out;
        }

        const std::string cooked_path =
            request.cooked_path.empty() ? buildDefaultCookedPath(request.source_path) : request.cooked_path;

        auto cooked_native = vfs.resolveForWrite(cooked_path);
        if (!cooked_native)
        {
            out.message = "SceneImporter: cannot resolve cooked path";
            return out;
        }

        std::error_code ec;
        std::filesystem::create_directories(cooked_native->parent_path(), ec);
        if (ec)
        {
            out.message = "SceneImporter: create cooked directory failed";
            return out;
        }

        // 这里选择“规范化输出”：解析 -> pretty dump，保证 cooked 文件格式稳定（利于 diff/调试）
        // 若你希望 cooked 更小更快，可改为原样写入 source_bytes 或 dump(0) 压缩。
        try
        {
            const auto j = nlohmann::json::parse(source_bytes.begin(), source_bytes.end());
            const std::string cooked_text = j.dump(4);

            std::ofstream ofs(*cooked_native, std::ios::binary | std::ios::trunc);
            if (!ofs)
            {
                out.message = "SceneImporter: open cooked file failed";
                return out;
            }

            ofs.write(cooked_text.data(), static_cast<std::streamsize>(cooked_text.size()));
            if (!ofs.good())
            {
                out.message = "SceneImporter: write cooked file failed";
                return out;
            }
        }
        catch (const std::exception& e)
        {
            out.message = std::string("SceneImporter: cook json failed: ") + e.what();
            return out;
        }

        // Registry meta（保持与 TextureImporter 一致）
        AssetMetadata meta{};
        if (const auto* existing = registry.findByPath(request.source_path))
            meta = *existing;
        else
            meta.id = registry.generateUniqueID();

        meta.type = AssetType::Scene;
        meta.source_path = request.source_path;
        meta.cooked_path = cooked_path;
        meta.hash = request.hash;
        meta.is_valid = true;

        out.success = true;
        out.primary_id = meta.id;
        out.assets.push_back(std::move(meta));
        HBD_CORE_INFO("{} import_completed source_path={} cooked_path={} asset_id={}",
                      kSceneImporterLogTag,
                      request.source_path,
                      cooked_path,
                      out.primary_id.value);
        return out;
    }

} // namespace Hybrid
