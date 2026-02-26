#include "project_loader.h"
#include <fstream>
#include <unordered_map>

namespace Hybrid {

    static std::string Trim(std::string s)
    {
        auto isws = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (!s.empty() && isws((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && isws((unsigned char)s.back()))  s.pop_back();
        return s;
    }

    static bool ParseKeyValueFile(const std::filesystem::path& p,
        std::unordered_map<std::string, std::string>& out,
        std::string& outError)
    {
        std::ifstream ifs(p);
        if (!ifs) { outError = "Failed to open hyproj: " + p.string(); return false; }

        std::string line;
        while (std::getline(ifs, line))
        {
            line = Trim(line);
            if (line.empty()) continue;
            if (line[0] == '#') continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            auto k = Trim(line.substr(0, pos));
            auto v = Trim(line.substr(pos + 1));
            if (!k.empty()) out[k] = v;
        }
        return true;
    }

    bool ProjectLoader::LoadFromFile(const std::filesystem::path& hyprojPath,
        ProjectContext& outCtx,
        std::string& outError)
    {
        namespace fs = std::filesystem;

        auto absProj = fs::absolute(hyprojPath);
        if (!fs::exists(absProj)) { outError = "hyproj not found: " + absProj.string(); return false; }

        std::unordered_map<std::string, std::string> kv;
        if (!ParseKeyValueFile(absProj, kv, outError)) return false;

        const fs::path root = absProj.parent_path();

        const fs::path assetsRel = kv.count("assets") ? kv["assets"] : "Assets";
        const fs::path cacheRel = kv.count("cache") ? kv["cache"] : "Cache";
        const fs::path buildRel = kv.count("build") ? kv["build"] : "Build";
        const fs::path settingsRel = kv.count("settings") ? kv["settings"] : "ProjectSettings";

        outCtx.root = fs::weakly_canonical(root);
        outCtx.assets = fs::weakly_canonical(root / assetsRel);
        outCtx.cache = fs::weakly_canonical(root / cacheRel);
        outCtx.build = fs::weakly_canonical(root / buildRel);
        outCtx.settings = fs::weakly_canonical(root / settingsRel);

        // Assets 必须存在（避免误选）
        if (!fs::exists(outCtx.assets))
        {
            outError = "Assets directory not found: " + outCtx.assets.string();
            return false;
        }

        // Cache / Build / Settings 自动创建
        std::error_code ec;
        fs::create_directories(outCtx.cache, ec);
        if (ec) { outError = "Failed to create Cache: " + outCtx.cache.string() + " (" + ec.message() + ")"; return false; }

        ec.clear();
        fs::create_directories(outCtx.build, ec);
        if (ec) { outError = "Failed to create Build: " + outCtx.build.string() + " (" + ec.message() + ")"; return false; }

        ec.clear();
        fs::create_directories(outCtx.settings, ec);
        if (ec) { outError = "Failed to create ProjectSettings: " + outCtx.settings.string() + " (" + ec.message() + ")"; return false; }

        return true;
    }

} // namespace Hybrid
