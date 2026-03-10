#include "asset_meta_store.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    using json = nlohmann::json;

    namespace
    {
        bool hasParentTraversal(const std::filesystem::path& p)
        {
            for (const auto& part : p)
            {
                if (part == "..")
                    return true;
            }
            return false;
        }

        bool parseLogicalPath(const std::string& input, std::string& alias_out, std::string& rel_out)
        {
            const auto colon_pos = input.find(':');
            if (colon_pos == std::string::npos || colon_pos == 0 || colon_pos + 1 >= input.size())
                return false;

            const bool is_windows_drive =
                (colon_pos == 1 && std::isalpha(static_cast<unsigned char>(input[0])) != 0);
            if (is_windows_drive)
                return false;

            std::string alias = input.substr(0, colon_pos);
            std::string rel = input.substr(colon_pos + 1);

            if (alias.empty() || rel.empty())
                return false;
            if (rel.front() == '/' || rel.front() == '\\')
                return false;

            for (char& ch : rel)
            {
                if (ch == '\\')
                    ch = '/';
            }
            if (rel.find(':') != std::string::npos)
                return false;

            std::filesystem::path rel_path(rel);
            if (rel_path.is_absolute())
                return false;

            rel_path = rel_path.lexically_normal();
            if (hasParentTraversal(rel_path))
                return false;

            std::string norm_rel = rel_path.generic_string();
            while (norm_rel.rfind("./", 0) == 0)
                norm_rel.erase(0, 2);

            if (norm_rel.empty() || norm_rel == ".")
                return false;

            alias_out = std::move(alias);
            rel_out = std::move(norm_rel);
            return true;
        }

        bool normalizeLogicalPath(const std::string& input, std::string& normalized_out)
        {
            std::string alias, rel;
            if (!parseLogicalPath(input, alias, rel))
                return false;
            normalized_out = alias + ":" + rel;
            return true;
        }

        bool parseAssetId(const json& jv, AssetID& out)
        {
            uint64_t id = 0;
            if (jv.is_string())
            {
                const auto s = jv.get<std::string>();
                if (s.empty())
                    return false;
                try
                {
                    size_t idx = 0;
                    id = std::stoull(s, &idx, 10);
                    if (idx != s.size())
                        return false;
                }
                catch (...)
                {
                    return false;
                }
            }
            else if (jv.is_number_unsigned())
            {
                id = jv.get<uint64_t>();
            }
            else if (jv.is_number_integer())
            {
                const auto i = jv.get<int64_t>();
                if (i < 0)
                    return false;
                id = static_cast<uint64_t>(i);
            }
            else
            {
                return false;
            }

            out = AssetID::FromRaw(id);
            return out.value != 0;
        }

        bool parseAssetIdArray(const json& jarr, std::vector<AssetID>& out)
        {
            if (!jarr.is_array())
                return false;

            out.clear();
            out.reserve(jarr.size());
            for (const auto& item : jarr)
            {
                AssetID id{};
                if (!parseAssetId(item, id))
                    return false;
                out.push_back(id);
            }
            return true;
        }
    } // namespace

    AssetMetaStore::AssetMetaStore(std::shared_ptr<AssetRegistry> registry) : m_registry(std::move(registry)) {}

    AssetMetaLoadResult AssetMetaStore::loadAll(const std::filesystem::path& assets_root)
    {
        AssetMetaLoadResult result{};

        if (!m_registry || assets_root.empty() || !std::filesystem::exists(assets_root))
            return result;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(assets_root, ec), end;
        if (ec)
        {
            HBD_CORE_ERROR("AssetMetaStore: cannot scan {} ({})", assets_root.string(), ec.message());
            return result;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                HBD_CORE_WARN("AssetMetaStore: scan error under {} ({})", assets_root.string(), ec.message());
                ec.clear();
                continue;
            }

            if (!it->is_regular_file())
                continue;
            if (it->path().extension() != ".meta")
                continue;

            ++result.total_files;

            AssetMetadata meta{};
            if (parseMetaFile(it->path(), meta))
            {
                m_registry->registerAsset(meta);
                ++result.loaded;
            }
            else
            {
                ++result.failed;
            }
        }

        return result;
    }

    bool AssetMetaStore::loadOne(const std::filesystem::path& meta_file, AssetMetadata& out) const
    {
        return parseMetaFile(meta_file, out);
    }

    bool AssetMetaStore::saveOne(const AssetMetadata& meta, const std::filesystem::path& assets_root)
    {
        if (!m_registry || assets_root.empty())
            return false;
        if (meta.id.value == 0 || meta.type == AssetType::Unknown)
            return false;
        if (!isValidLogicalPath(meta.source_path))
            return false;
        if (!meta.cooked_path.empty() && !isValidLogicalPath(meta.cooked_path))
            return false;
        if ((meta.parent_id.value == 0) != (meta.subasset_key.empty()))
            return false;

        std::string source_alias, source_rel;
        if (!parseLogicalPath(meta.source_path, source_alias, source_rel))
            return false;
        if (source_alias != "asset")
            return false;

        AssetMetadata canonical = meta;
        if (!normalizeLogicalPath(meta.source_path, canonical.source_path))
            return false;
        if (!meta.cooked_path.empty() && !normalizeLogicalPath(meta.cooked_path, canonical.cooked_path))
            return false;

        const auto meta_file = metaPathFromSource(meta.source_path, assets_root);
        if (meta_file.empty())
            return false;

        return writeMetaFileAtomic(meta_file, canonical);
    }

    bool AssetMetaStore::removeOne(const std::string& source_path, const std::filesystem::path& assets_root)
    {
        const auto meta_file = metaPathFromSource(source_path, assets_root);
        if (meta_file.empty())
            return false;

        std::error_code ec;
        const bool removed = std::filesystem::remove(meta_file, ec);
        return !ec && removed;
    }

    std::filesystem::path AssetMetaStore::metaPathFromSource(const std::string& source_path,
                                                             const std::filesystem::path& assets_root)
    {
        if (assets_root.empty())
            return {};

        std::string alias, rel;
        if (!parseLogicalPath(source_path, alias, rel))
            return {};
        if (alias != "asset")
            return {};

        std::filesystem::path file = assets_root / std::filesystem::path(rel);
        file += ".meta";
        return file;
    }

    bool AssetMetaStore::parseMetaFile(const std::filesystem::path& file, AssetMetadata& out) const
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            HBD_CORE_WARN("AssetMetaStore: cannot open meta {}", file.string());
            return false;
        }

        json j = json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object())
        {
            HBD_CORE_WARN("AssetMetaStore: invalid json {}", file.string());
            return false;
        }

        if (!j.contains("version") || !j["version"].is_number_integer())
        {
            HBD_CORE_WARN("AssetMetaStore: missing version {}", file.string());
            return false;
        }
        if (j["version"].get<int>() != 1)
        {
            HBD_CORE_WARN("AssetMetaStore: unsupported version {} in {}", j["version"].get<int>(), file.string());
            return false;
        }

        AssetMetadata meta{};

        if (!j.contains("id") || !parseAssetId(j["id"], meta.id))
        {
            HBD_CORE_WARN("AssetMetaStore: invalid id {}", file.string());
            return false;
        }

        if (!j.contains("type") || !j["type"].is_string())
        {
            HBD_CORE_WARN("AssetMetaStore: missing type {}", file.string());
            return false;
        }
        meta.type = AssetTypeFromString(j["type"].get<std::string>());
        if (meta.type == AssetType::Unknown)
        {
            HBD_CORE_WARN("AssetMetaStore: unknown type in {}", file.string());
            return false;
        }

        if (!j.contains("source_path") || !j["source_path"].is_string())
        {
            HBD_CORE_WARN("AssetMetaStore: missing source_path {}", file.string());
            return false;
        }
        meta.source_path = j["source_path"].get<std::string>();
        if (!isValidLogicalPath(meta.source_path))
        {
            HBD_CORE_WARN("AssetMetaStore: invalid source_path {} in {}", meta.source_path, file.string());
            return false;
        }
        std::string source_alias, source_rel;
        if (!parseLogicalPath(meta.source_path, source_alias, source_rel) || source_alias != "asset")
        {
            HBD_CORE_WARN("AssetMetaStore: source_path must use asset: alias in {}", file.string());
            return false;
        }
        meta.source_path = source_alias + ":" + source_rel;

        if (j.contains("cooked_path") && j["cooked_path"].is_string())
        {
            meta.cooked_path = j["cooked_path"].get<std::string>();
            if (!meta.cooked_path.empty() && !isValidLogicalPath(meta.cooked_path))
            {
                HBD_CORE_WARN("AssetMetaStore: invalid cooked_path {} in {}", meta.cooked_path, file.string());
                return false;
            }
            if (!meta.cooked_path.empty())
            {
                std::string cooked_norm;
                if (!normalizeLogicalPath(meta.cooked_path, cooked_norm))
                    return false;
                meta.cooked_path = std::move(cooked_norm);
            }
        }

        if (j.contains("hash") && j["hash"].is_string())
            meta.hash = j["hash"].get<std::string>();

        if (j.contains("parent_id"))
        {
            if (!parseAssetId(j["parent_id"], meta.parent_id))
            {
                HBD_CORE_WARN("AssetMetaStore: invalid parent_id {}", file.string());
                return false;
            }
        }

        if (j.contains("subasset_key"))
        {
            if (!j["subasset_key"].is_string())
            {
                HBD_CORE_WARN("AssetMetaStore: invalid subasset_key {}", file.string());
                return false;
            }
            meta.subasset_key = j["subasset_key"].get<std::string>();
        }

        if ((meta.parent_id.value == 0) != (meta.subasset_key.empty()))
        {
            HBD_CORE_WARN("AssetMetaStore: parent_id/subasset_key mismatch {}", file.string());
            return false;
        }

        if (j.contains("hard_deps"))
        {
            if (!parseAssetIdArray(j["hard_deps"], meta.hard_deps))
            {
                HBD_CORE_WARN("AssetMetaStore: invalid hard_deps {}", file.string());
                return false;
            }
        }

        if (j.contains("soft_deps"))
        {
            if (!parseAssetIdArray(j["soft_deps"], meta.soft_deps))
            {
                HBD_CORE_WARN("AssetMetaStore: invalid soft_deps {}", file.string());
                return false;
            }
        }

        meta.is_valid = true;
        if (j.contains("is_valid") && j["is_valid"].is_boolean())
            meta.is_valid = j["is_valid"].get<bool>();

        out = std::move(meta);
        return true;
    }

    bool AssetMetaStore::writeMetaFileAtomic(const std::filesystem::path& file, const AssetMetadata& meta) const
    {
        json j;
        j["version"] = 1;
        j["id"] = std::to_string(meta.id.value);
        j["type"] = AssetTypeToString(meta.type);
        j["source_path"] = meta.source_path;
        j["cooked_path"] = meta.cooked_path;
        j["hash"] = meta.hash;
        if (meta.parent_id.value != 0)
            j["parent_id"] = std::to_string(meta.parent_id.value);
        if (!meta.subasset_key.empty())
            j["subasset_key"] = meta.subasset_key;
        j["is_valid"] = meta.is_valid;

        j["hard_deps"] = json::array();
        for (const auto& dep : meta.hard_deps)
            j["hard_deps"].push_back(std::to_string(dep.value));

        j["soft_deps"] = json::array();
        for (const auto& dep : meta.soft_deps)
            j["soft_deps"].push_back(std::to_string(dep.value));

        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        if (ec)
        {
            HBD_CORE_ERROR("AssetMetaStore: create directory failed {} ({})", file.parent_path().string(), ec.message());
            return false;
        }

        std::filesystem::path tmp = file;
        tmp += ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out)
                return false;
            out << j.dump(2);
            if (!out.good())
                return false;
        }

        std::error_code rm_ec;
        std::filesystem::remove(file, rm_ec);

        std::error_code mv_ec;
        std::filesystem::rename(tmp, file, mv_ec);
        if (mv_ec)
        {
            std::filesystem::remove(tmp, rm_ec);
            HBD_CORE_ERROR("AssetMetaStore: atomic rename failed {} ({})", file.string(), mv_ec.message());
            return false;
        }

        return true;
    }

    bool AssetMetaStore::isValidLogicalPath(const std::string& path) const
    {
        std::string alias, rel;
        return parseLogicalPath(path, alias, rel);
    }
} // namespace Hybrid
