#pragma once
#include <filesystem>

namespace Hybrid
{
    class Scene;

    class SceneSerializer
    {
    public:
        static bool SerializeToFile(const Scene& scene, const std::filesystem::path& path);
        static bool DeserializeFromFile(Scene& scene, const std::filesystem::path& path);
    };
} // namespace Hybrid
