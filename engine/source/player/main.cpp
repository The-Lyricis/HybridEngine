#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/runtime/engine.h"

namespace
{
    struct PlayerOptions
    {
        std::filesystem::path project;
        std::string scene;
        bool headless = false;
        uint64_t max_frames = 0;
    };

    void printUsage()
    {
        std::cerr << "Usage: HybridPlayer --project <file.hyproj> [--scene <logical-path>] "
                     "[--headless] [--max-frames <N>]\n";
    }

    bool parseUnsigned(const std::string& value, uint64_t& output)
    {
        if (value.empty())
            return false;
        const char* begin = value.data();
        const char* end = begin + value.size();
        const auto result = std::from_chars(begin, end, output);
        return result.ec == std::errc{} && result.ptr == end;
    }

    bool parseOptions(int argc, char** argv, PlayerOptions& options, std::string& error)
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--project" || argument == "--scene" || argument == "--max-frames")
            {
                if (++index >= argc)
                {
                    error = "missing value after " + argument;
                    return false;
                }
                const std::string value = argv[index];
                if (argument == "--project")
                    options.project = value;
                else if (argument == "--scene")
                    options.scene = value;
                else if (!parseUnsigned(value, options.max_frames))
                {
                    error = "invalid --max-frames value: " + value;
                    return false;
                }
            }
            else if (argument == "--headless")
            {
                options.headless = true;
            }
            else
            {
                error = "unknown argument: " + argument;
                return false;
            }
        }

        if (options.project.empty())
        {
            error = "--project is required";
            return false;
        }
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    PlayerOptions options{};
    std::string error;
    if (!parseOptions(argc, argv, options, error))
    {
        std::cerr << "HybridPlayer: " << error << '\n';
        printUsage();
        return 2;
    }

    Hybrid::HybridEngine engine;
    Hybrid::EngineConfig config{};
    config.project_path = options.project;
    config.headless = options.headless;
    if (!engine.initialize(config))
    {
        std::cerr << "HybridPlayer: failed to initialize project: "
                  << options.project.generic_string() << '\n';
        return 3;
    }

    const Hybrid::ProjectContext& project = Hybrid::ProjectService::Get();
    const std::string scene_path = !options.scene.empty() ? options.scene : project.startup_scene;
    if (scene_path.empty())
    {
        std::cerr << "HybridPlayer: no startup scene; pass --scene <logical-path> or set startup_scene in "
                  << project.project_file.generic_string() << '\n';
        engine.shutdown();
        return 4;
    }

    const auto registry = engine.getResourceSystem().getRegistry();
    const auto manager = engine.getResourceSystem().getManager();
    const auto metadata = registry ? registry->findByPath(scene_path) : std::nullopt;
    if (!metadata)
    {
        std::cerr << "HybridPlayer: startup scene is not registered: " << scene_path << '\n';
        engine.shutdown();
        return 5;
    }
    if (metadata->type != Hybrid::AssetType::Scene)
    {
        std::cerr << "HybridPlayer: startup path is not a Scene asset: " << scene_path << '\n';
        engine.shutdown();
        return 6;
    }

    const auto scene = manager ? manager->loadSync<Hybrid::Scene>(metadata->id) : nullptr;
    if (!scene)
    {
        std::cerr << "HybridPlayer: failed to load startup scene: " << scene_path << '\n';
        engine.shutdown();
        return 7;
    }
    if (!engine.setActiveScene(scene))
    {
        std::cerr << "HybridPlayer: failed to activate startup scene: " << scene_path << '\n';
        engine.shutdown();
        return 8;
    }

    engine.setFixedUpdateEnabled(true);

    engine.run(options.max_frames);
    engine.shutdown();
    return 0;
}
