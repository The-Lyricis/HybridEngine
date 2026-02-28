#include "engine.h"

#include <algorithm>
#include <string>

#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/core/log/log_system.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "runtime/modules/project/project_loader.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/scene/scene_serializer.h"

namespace Hybrid
{
    static void EnsureDemoScenes(const std::filesystem::path& assetsDir)
    {
        namespace fs = std::filesystem;
        using json = nlohmann::json;

        const fs::path scenesDir = assetsDir / "Scenes";
        const fs::path aPath = scenesDir / "DemoA.scene";
        const fs::path bPath = scenesDir / "DemoB.scene";

        std::error_code ec;
        fs::create_directories(scenesDir, ec);
        if (ec)
            return;

        auto write = [](const fs::path& path, const json& root) -> bool
            {
                std::ofstream ofs(path, std::ios::out | std::ios::trunc);
                if (!ofs)
                    return false;
                ofs << root.dump(4);
                return true;
            };

        auto makeRoot = []() -> json
            {
                json root;
                root["meta"] = { {"version", 2} };
                root["entities"] = json::array();
                return root;
            };

        auto pushEntity = [](json& root,
            uint64_t uuid,
            const std::string& name,
            uint64_t parent_uuid,
            const glm::vec3& t,
            const glm::quat& r,
            const glm::vec3& s,
            bool addMeshRenderer,
            int primitive,
            const glm::vec4& tint,
            bool addCamera,
            bool cameraPrimary,
            float cameraFovY,
            float cameraNear,
            float cameraFar,
            bool addDirLight,
            const glm::vec3& lightColor,
            float lightIntensity)
            {
                json e;
                e["uuid"] = uuid;
                e["name"] = name;
                e["parent"] = parent_uuid;

                e["transform"] = {
                    {"t", json::array({t.x, t.y, t.z})},
                    {"r", json::array({r.w, r.x, r.y, r.z})}, // [w,x,y,z]
                    {"s", json::array({s.x, s.y, s.z})}
                };

                if (addMeshRenderer)
                {
                    e["meshRenderer"] = {
                        {"mesh", 0ull},        // AssetID.value，0 表示无（走默认/primitive）
                        {"material", 0ull},
                        {"primitive", primitive},
                        {"tint", json::array({tint.x, tint.y, tint.z, tint.w})}
                    };
                }

                if (addCamera)
                {
                    e["camera"] = {
                        {"primary", cameraPrimary},
                        {"fovY", cameraFovY},
                        {"near", cameraNear},
                        {"far",  cameraFar}
                    };
                }

                if (addDirLight)
                {
                    e["dirLight"] = {
                        {"color", json::array({lightColor.x, lightColor.y, lightColor.z})},
                        {"intensity", lightIntensity}
                    };
                }

                root["entities"].push_back(std::move(e));
            };

        // =========================================================
        // DemoA：5x5 网格 + 默认相机 + 默认方向光
        // =========================================================
        {
            json root = makeRoot();

            const uint64_t rootId = 1000;

            // Root_A（不渲染）
            pushEntity(root,
                rootId,
                "Root_A",
                0,
                glm::vec3{ 0.0f, 0.0f, 0.0f },
                glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f },
                glm::vec3{ 1.0f, 1.0f, 1.0f },
                /*addMeshRenderer=*/false,
                /*primitive=*/0,
                glm::vec4{ 1.0f },
                /*addCamera=*/false,
                /*cameraPrimary=*/false,
                45.0f, 0.1f, 500.0f,
                /*addDirLight=*/false,
                glm::vec3{ 1.0f, 1.0f, 1.0f },
                1.0f);

            // Camera_A（Primary）
            {
                const uint64_t camId = 1100;
                const glm::vec3 t{ 0.0f, 12.0f, 12.0f };
                const glm::quat r = MathUtil::quatFromEulerRadians({ glm::radians(-45.0f), 0.0f, 0.0f });
                const glm::vec3 s{ 1.0f, 1.0f, 1.0f };

                pushEntity(root,
                    camId,
                    "Camera_A",
                    0,
                    t, r, s,
                    /*addMeshRenderer=*/false,
                    0,
                    glm::vec4{ 1.0f },
                    /*addCamera=*/true,
                    /*cameraPrimary=*/true,
                    45.0f, 0.1f, 500.0f,
                    /*addDirLight=*/false,
                    glm::vec3{ 1.0f, 1.0f, 1.0f },
                    1.0f);
            }

            // Sun_A（Directional Light）
            {
                const uint64_t sunId = 1101;
                const glm::vec3 t{ 0.0f, 0.0f, 0.0f };
                const glm::quat r = MathUtil::quatFromEulerRadians({ glm::radians(-70.5f), glm::radians(-123.7f), 0.0f });
                const glm::vec3 s{ 1.0f, 1.0f, 1.0f };

                pushEntity(root,
                    sunId,
                    "Sun_A",
                    0,
                    t, r, s,
                    /*addMeshRenderer=*/false,
                    0,
                    glm::vec4{ 1.0f },
                    /*addCamera=*/false,
                    false,
                    45.0f, 0.1f, 500.0f,
                    /*addDirLight=*/true,
                    glm::vec3{ 1.0f, 1.0f, 1.0f },
                    1.0f);
            }

            // 5x5 Cubes（渲染）
            const int gridX = 5;
            const int gridZ = 5;
            const float spacing = 2.0f;
            const float startX = -0.5f * (gridX - 1) * spacing;
            const float startZ = -0.5f * (gridZ - 1) * spacing;

            uint64_t uid = rootId + 1;
            for (int z = 0; z < gridZ; ++z)
            {
                for (int x = 0; x < gridX; ++x)
                {
                    const glm::vec3 t{ startX + x * spacing, 0.0f, startZ + z * spacing };
                    const glm::quat r{ 1.0f, 0.0f, 0.0f, 0.0f };
                    const glm::vec3 s{ 1.0f, 1.0f, 1.0f };
                    const glm::vec4 tint{ 0.85f, 0.90f, 1.0f, 1.0f };

                    pushEntity(root,
                        uid++,
                        "A_Cube_" + std::to_string(z) + "_" + std::to_string(x),
                        rootId,
                        t, r, s,
                        /*addMeshRenderer=*/true,
                        /*primitive=*/0,
                        tint,
                        /*addCamera=*/false,
                        false,
                        45.0f, 0.1f, 500.0f,
                        /*addDirLight=*/false,
                        glm::vec3{ 1.0f, 1.0f, 1.0f },
                        1.0f);
                }
            }

            (void)write(aPath, root);
        }

        // =========================================================
        // DemoB：斜坡队列 + 两级层级 + 默认相机 + 默认方向光
        // =========================================================
        {
            json root = makeRoot();

            const uint64_t rootId = 2000;
            const uint64_t groupId = 2001;

            // Root_B（不渲染）
            pushEntity(root,
                rootId,
                "Root_B",
                0,
                glm::vec3{ 0.0f, 0.0f, 0.0f },
                glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f },
                glm::vec3{ 1.0f, 1.0f, 1.0f },
                /*addMeshRenderer=*/false,
                0,
                glm::vec4{ 1.0f },
                /*addCamera=*/false,
                false,
                45.0f, 0.1f, 500.0f,
                /*addDirLight=*/false,
                glm::vec3{ 1.0f, 1.0f, 1.0f },
                1.0f);

            // Group_B（不渲染）
            pushEntity(root,
                groupId,
                "Group_B",
                rootId,
                glm::vec3{ 0.0f, 0.0f, 0.0f },
                glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f },
                glm::vec3{ 1.0f, 1.0f, 1.0f },
                /*addMeshRenderer=*/false,
                0,
                glm::vec4{ 1.0f },
                /*addCamera=*/false,
                false,
                45.0f, 0.1f, 500.0f,
                /*addDirLight=*/false,
                glm::vec3{ 1.0f, 1.0f, 1.0f },
                1.0f);

            // Camera_B（Primary）
            {
                const uint64_t camId = 2100;
                const glm::vec3 t{ 0.0f, 10.0f, 16.0f };
                const glm::quat r = MathUtil::quatFromEulerRadians({ glm::radians(-35.0f), glm::radians(20.0f), 0.0f });
                const glm::vec3 s{ 1.0f, 1.0f, 1.0f };

                pushEntity(root,
                    camId,
                    "Camera_B",
                    0,
                    t, r, s,
                    /*addMeshRenderer=*/false,
                    0,
                    glm::vec4{ 1.0f },
                    /*addCamera=*/true,
                    /*cameraPrimary=*/true,
                    45.0f, 0.1f, 500.0f,
                    /*addDirLight=*/false,
                    glm::vec3{ 1.0f, 1.0f, 1.0f },
                    1.0f);
            }

            // Sun_B（Directional Light）
            {
                const uint64_t sunId = 2101;
                const glm::vec3 t{ 0.0f, 0.0f, 0.0f };
                const glm::quat r = MathUtil::quatFromEulerRadians({ glm::radians(-60.0f), glm::radians(-30.0f), 0.0f });
                const glm::vec3 s{ 1.0f, 1.0f, 1.0f };

                pushEntity(root,
                    sunId,
                    "Sun_B",
                    0,
                    t, r, s,
                    /*addMeshRenderer=*/false,
                    0,
                    glm::vec4{ 1.0f },
                    /*addCamera=*/false,
                    false,
                    45.0f, 0.1f, 500.0f,
                    /*addDirLight=*/true,
                    glm::vec3{ 1.0f, 1.0f, 1.0f },
                    1.0f);
            }

            // Blocks（渲染，挂在 Group_B 下）
            uint64_t uid = 2002;
            for (int i = 0; i < 12; ++i)
            {
                const glm::vec3 t{ -8.0f + i * 1.5f, 0.2f * i, -2.0f + i * 0.6f };
                const glm::vec3 s{ 1.0f, 1.0f, 1.0f };

                const float yaw = glm::radians(8.0f * static_cast<float>(i));
                const glm::quat r = MathUtil::quatFromEulerRadians({ 0.0f, yaw, 0.0f });

                const glm::vec4 tint{ 1.0f, 0.90f, 0.75f, 1.0f };

                pushEntity(root,
                    uid++,
                    "B_Block_" + std::to_string(i),
                    groupId,
                    t, r, s,
                    /*addMeshRenderer=*/true,
                    /*primitive=*/0,
                    tint,
                    /*addCamera=*/false,
                    false,
                    45.0f, 0.1f, 500.0f,
                    /*addDirLight=*/false,
                    glm::vec3{ 1.0f, 1.0f, 1.0f },
                    1.0f);
            }

            (void)write(bPath, root);
        }
    }

   
    void HybridEngine::initialize()
    {
        LogSystem::initialize();

        // ===== Project Bootstrap (CWD/GameProject) =====
        namespace fs = std::filesystem;

        const fs::path outputDir = fs::current_path();
        const fs::path projectRoot = outputDir / "GameProject";
        const fs::path hyprojPath = projectRoot / "GameProject.hyproj";

        const fs::path assetsDir = projectRoot / "Assets";
        const fs::path cacheDir = projectRoot / "Cache";
        const fs::path buildDir = projectRoot / "Build";
        const fs::path settingsDir = projectRoot / "ProjectSettings";

        std::error_code ec;

        // 1) Create required directories.
        fs::create_directories(projectRoot, ec);
        if (ec)
        {
            HBD_CORE_ERROR("Failed to create ProjectRoot: {} ({})", projectRoot.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(assetsDir, ec);
        if (ec)
        {
            HBD_CORE_ERROR("Failed to create Assets dir: {} ({})", assetsDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(cacheDir, ec);
        if (ec)
        {
            HBD_CORE_ERROR("Failed to create Cache dir: {} ({})", cacheDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(buildDir, ec);
        if (ec)
        {
            HBD_CORE_ERROR("Failed to create Build dir: {} ({})", buildDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(settingsDir, ec);
        if (ec)
        {
            HBD_CORE_ERROR("Failed to create ProjectSettings dir: {} ({})", settingsDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        // 2) Generate a default hyproj if missing.
        if (!fs::exists(hyprojPath))
        {
            std::ofstream ofs(hyprojPath, std::ios::out | std::ios::binary);
            if (!ofs)
            {
                HBD_CORE_ERROR("Failed to create hyproj: {}", hyprojPath.string());
                LogSystem::shutdown();
                return;
            }

            ofs << "# Auto-generated project file (debug)\n";
            ofs << "name=GameProject\n";
            ofs << "assets=Assets\n";
            ofs << "cache=Cache\n";
            ofs << "build=Build\n";
            ofs << "settings=ProjectSettings\n";
            ofs.close();

            HBD_CORE_INFO("Created default hyproj: {}", fs::absolute(hyprojPath).string());
        }

        // 3) Load hyproj into ProjectContext and publish ProjectService.
        Hybrid::ProjectContext pctx;
        std::string perr;
        if (!Hybrid::ProjectLoader::LoadFromFile(hyprojPath, pctx, perr))
        {
            HBD_CORE_ERROR("Project load failed: {}", perr);
            HBD_CORE_ERROR("hyproj: {}", fs::absolute(hyprojPath).string());
            LogSystem::shutdown();
            return;
        }

        Hybrid::ProjectService::Set(pctx);
        EnsureDemoScenes(pctx.assets);


        HBD_CORE_INFO("Project loaded: {}", fs::absolute(hyprojPath).string());
        HBD_CORE_INFO("Project Root   : {}", pctx.root.string());
        HBD_CORE_INFO("Assets         : {}", pctx.assets.string());
        HBD_CORE_INFO("Cache          : {}", pctx.cache.string());
        HBD_CORE_INFO("Build          : {}", pctx.build.string());
        HBD_CORE_INFO("ProjectSettings: {}", pctx.settings.string());
        // =============================================

        // ===== Window / Graphics =====
        m_Window = std::make_shared<WindowSystem>();
        m_Window->initialize(1280, 720, "Hybrid Engine");

        GLFWwindow *window = m_Window->getNativeWindow();
        if (!window)
        {
            HBD_CORE_ERROR("GLFW window is null.");
            m_Window->cleanup();
            LogSystem::shutdown();
            return;
        }

        m_GraphicsContext = GraphicsContext::Create(window);
        if (!m_GraphicsContext)
        {
            HBD_CORE_ERROR("GraphicsContext creation failed.");
            m_Window->cleanup();
            LogSystem::shutdown();
            return;
        }
        m_GraphicsContext->init();

        // ===== Resource System (Project-based) =====
        m_RuntimeResourceSystem = std::make_shared<RuntimeResourceSystem>();

        // Pass nullptr VFS to let RuntimeResourceSystem create the default NativeFileSystem.
        m_RuntimeResourceSystem->initialize(Hybrid::ProjectService::Get(), nullptr);

        m_RenderSystem.setAssetManager(m_RuntimeResourceSystem->getManager());

        // ===== Event / Layers =====
        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event &e)
                                        { onEvent(e); });

        m_InputLayer = new InputLayer();
        pushLayer(m_InputLayer);

        // ===== Render =====
        m_RenderSystem.initialize(window);

        // ===== Scene =====
        auto scene = std::make_shared<Scene>();
        m_SceneManager.setActiveScene(scene);
        m_RenderSystem.setScene(scene);
        m_FrameContext.scene = scene;
        m_FrameContext.window_handle = window;

        // ===== Hardcoded Scene Setup for Demo =====
        {
            auto cam = scene->createEntity("Game Camera");
            cam.AddComponent<Hybrid::CameraComponent>(Hybrid::CameraComponent{true, 45.0f, 0.1f, 500.0f});

            auto &tr = cam.GetComponent<Hybrid::TransformComponent>();
            tr.Position = {0.0f, 12.0f, 12.0f};
            tr.Rotation = MathUtil::quatFromEulerRadians({glm::radians(-45.0f), 0.0f, 0.0f});
            tr.Scale = {1.0f, 1.0f, 1.0f};

            auto sun = scene->createEntity("Sun");
            auto &dl = sun.AddComponent<Hybrid::DirectionalLightComponent>();
            dl.Color = {1.0f, 1.0f, 1.0f};
            dl.Intensity = 1.0f;
            auto &sunTr = sun.GetComponent<Hybrid::TransformComponent>();
            sunTr.Rotation = MathUtil::quatFromEulerRadians({glm::radians(-70.5f), glm::radians(-123.7f), 0.0f});

            const int gridX = 5;
            const int gridZ = 5;
            const float spacing = 2.0f;
            const float startX = -0.5f * (gridX - 1) * spacing;
            const float startZ = -0.5f * (gridZ - 1) * spacing;

            for (int z = 0; z < gridZ; ++z)
            {
                for (int x = 0; x < gridX; ++x)
                {
                    std::string name = "Cube_" + std::to_string(z) + "_" + std::to_string(x);
                    auto cube = scene->createEntity(name);

                    auto& mr = cube.AddComponent<Hybrid::MeshRendererComponent>();
                    mr.Primitive = 0;

                    auto& tr = cube.GetComponent<Hybrid::TransformComponent>();
                    tr.Position = { startX + x * spacing, 0.0f, startZ + z * spacing };
                    tr.Rotation = glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f };
                    tr.Scale = { 1.0f, 1.0f, 1.0f };
                }
            }

            if (m_RuntimeResourceSystem && m_RuntimeResourceSystem->getRegistry())
{
            const auto* mesh_meta = m_RuntimeResourceSystem->getRegistry()->findByPath("asset:Model/rock-a.obj");
            if (mesh_meta)
            {
                auto rock = scene->createEntity("ROCK-A");
                auto& mr = rock.AddComponent<Hybrid::MeshRendererComponent>();
                mr.Mesh = mesh_meta->id;   // 关键：绑定导入出来的 Mesh 资产
                mr.Primitive = 0;

                auto& tr = rock.GetComponent<Hybrid::TransformComponent>();
                tr.Position = {0.0f, 0.0f, 0.0f};
                tr.Scale = {1.0f, 1.0f, 1.0f};
            }
            else
            {
                HBD_CORE_WARN("rock-a.obj meta not found in registry");
            }
}
        }

        // 绑定到系统
        m_SceneManager.setActiveScene(scene);
        m_RenderSystem.setScene(scene);
        m_FrameContext.scene = scene;
        m_FrameContext.window_handle = window;

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        m_FrameContext.viewport_size.x = static_cast<float>(std::max(1, fbw));
        m_FrameContext.viewport_size.y = static_cast<float>(std::max(1, fbh));

        m_LastTime = static_cast<float>(glfwGetTime());
    }

    void HybridEngine::run()
    {
        while (m_Running && !m_Window->shouldClose())
        {
            const float dt = calculateDeltaTime();
            m_Window->pollEvents();

            for (Layer *layer : m_LayerStack)
            {
                layer->onBeginFrame();
            }

            if (m_Minimized)
            {
                for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
                {
                    (*it)->onEndFrame();
                }
                m_GraphicsContext->swapBuffers();
                continue;
            }

            for (Layer *layer : m_LayerStack)
            {
                layer->onUpdate(dt);
            }

            if (auto scene = m_SceneManager.getActiveScene())
            {
                scene->onUpdate(dt);
            }

            m_FrameContext.dt = dt;
            m_FrameContext.input = &m_InputLayer->getState();
            m_FrameContext.scene = m_SceneManager.getActiveScene();

            glm::vec2 viewport_size = m_FrameContext.viewport_size;
            if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f)
            {
                int fbw = 0, fbh = 0;
                glfwGetFramebufferSize(m_Window->getNativeWindow(), &fbw, &fbh);
                viewport_size.x = static_cast<float>(std::max(1, fbw));
                viewport_size.y = static_cast<float>(std::max(1, fbh));
                m_FrameContext.viewport_size = viewport_size;
            }

            m_RenderSystem.renderFrame(m_FrameContext, m_RenderFlags, &m_EditorRenderExt);

            for (Layer *layer : m_LayerStack)
            {
                layer->onImGuiRender();
            }

            if (m_EditorRenderExt.request_pick && HasFlag(m_RenderFlags, RenderFlags::PickingID))
            {
                m_LastPickResult = m_RenderSystem.readEntityID(m_EditorRenderExt.pick_x, m_EditorRenderExt.pick_y);
                m_HasPendingPickResult = true;
                m_EditorRenderExt.request_pick = false;
            }

            for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
            {
                (*it)->onEndFrame();
            }

            m_GraphicsContext->swapBuffers();
        }
    }

    void HybridEngine::onEvent(Event &e)
    {
        if (m_InputLayer)
        {
            m_InputLayer->onEvent(e);
        }

        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &)
                                              {
            m_Running = false;
            return true; });

        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent &ev)
                                               {
            if (ev.getWidth() == 0 || ev.getHeight() == 0)
            {
                m_Minimized = true;
                return false;
            }

            m_Minimized = false;
            m_RenderSystem.onWindowResize(static_cast<uint32_t>(ev.getWidth()), static_cast<uint32_t>(ev.getHeight()));
            m_FrameContext.viewport_size = {static_cast<float>(ev.getWidth()), static_cast<float>(ev.getHeight())};
            return false; });

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            (*it)->onEvent(e);
            if (e.Handled)
            {
                break;
            }
        }
    }

    void HybridEngine::pushLayer(Layer *layer)
    {
        if (!layer)
            return;
        m_LayerStack.pushLayer(layer);
        layer->onAttach();
    }

    void HybridEngine::pushOverlay(Layer *layer)
    {
        if (!layer)
            return;
        m_LayerStack.pushOverlay(layer);
        layer->onAttach();
    }

    bool HybridEngine::consumePickResult(uint32_t &out_entity_id)
    {
        if (!m_HasPendingPickResult)
            return false;

        out_entity_id = m_LastPickResult;
        m_HasPendingPickResult = false;
        return true;
    }

    void HybridEngine::shutdown()
    {
        m_LayerStack.clear();
        m_InputLayer = nullptr;

        if (m_Window)
        {
            m_Window->cleanup();
        }

        LogSystem::shutdown();
    }

    float HybridEngine::calculateDeltaTime()
    {
        const float time = static_cast<float>(glfwGetTime());
        const float dt = time - m_LastTime;
        m_LastTime = time;
        return dt;
    }
} // namespace Hybrid
