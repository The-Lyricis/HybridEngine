#pragma once
#include "project_context.h"
#include <filesystem>
#include <string>

namespace Hybrid {

    class ProjectLoader
    {
    public:
        // 从 .hyproj 文件加载并生成 ProjectContext
        static bool LoadFromFile(const std::filesystem::path& hyprojPath,
            ProjectContext& outCtx,
            std::string& outError);
    };

} // namespace Hybrid
