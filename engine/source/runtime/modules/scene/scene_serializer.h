#pragma once
#include <filesystem>

namespace Hybrid
{
    class AssetRegistry;
    class Scene;

    class SceneSerializer
    {
    public:
        static bool SerializeToFile(const Scene& scene, const std::filesystem::path& path, const AssetRegistry* registry = nullptr);
        static bool DeserializeFromFile(Scene& scene, const std::filesystem::path& path, const AssetRegistry* registry = nullptr);
    };
} // namespace Hybrid
