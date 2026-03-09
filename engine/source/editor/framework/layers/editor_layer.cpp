#include "editor_layer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <entt/entity/entity.hpp>
#include <nlohmann/json.hpp>
#include <system_error>
#include <utility>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "editor/core/editor_context.h"
#include "editor/platform/windows/file_dialogs_win32.h"
#include "editor/services/asset/editor_resource_system.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/input/input_layer.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/scene/scene_manager.h"
#include "runtime/modules/window/window_system.h"
#include "runtime/modules/scene/scene_serializer.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;

        std::filesystem::path GetEditorUserSettingsPath()
        {
            const auto& project = ProjectService::Get();
            return project.root / "UserSettings" / "EditorState.json";
        }

        std::filesystem::path GetProjectSettingsPath()
        {
            const auto& project = ProjectService::Get();
            return project.settings / "ProjectSettings.json";
        }

        std::string trimCopy(std::string value)
        {
            auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
            value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
            return value;
        }

        bool normalizeSceneSaveVPath(const std::string& input, std::string& out_vpath)
        {
            std::string value = trimCopy(input);
            if (value.empty())
                return false;

            for (char& ch : value)
            {
                if (ch == '\\')
                    ch = '/';
            }

            constexpr const char* kAssetPrefix = "asset:";
            if (value.rfind(kAssetPrefix, 0) == 0)
                value.erase(0, std::char_traits<char>::length(kAssetPrefix));

            while (!value.empty() && value.front() == '/')
                value.erase(value.begin());
            if (value.empty())
                return false;

            std::filesystem::path rel_path(value);
            if (rel_path.is_absolute())
                return false;

            if (!rel_path.has_extension())
                rel_path += ".scene";
            else if (rel_path.extension() != ".scene")
                return false;

            rel_path = rel_path.lexically_normal();
            for (const auto& part : rel_path)
            {
                if (part == "..")
                    return false;
            }

            const std::string rel_string = rel_path.generic_string();
            if (rel_string.empty())
                return false;

            out_vpath = std::string(kAssetPrefix) + rel_string;
            return true;
        }
    } // namespace

    EditorLayer::EditorLayer(EngineServices services) : Layer("EditorLayer"), m_services(std::move(services)) {}
    void EditorLayer::onAttach()
    {
        if (!m_services.window || !m_services.render || !m_services.scene ||
            !m_services.frame_context || !m_services.render_flags || !m_services.editor_ext)
        {
            HBD_CORE_ERROR("EditorLayer attach failed: missing required engine services");
            return;
        }

        m_editor_ui.initialize(m_services.window->getNativeWindow());
      
        bindAssetChangeCallback();

        if (m_services.resources && m_services.resources->getRegistry())
        {
            m_assets_root = m_services.resources->getRegistry()->getRoot();
            if (!m_assets_root.empty())
            {
                if (!m_file_watcher.initialize(m_assets_root, true))
                {
                    HBD_CORE_WARN("EditorLayer: file watcher init failed at {}", m_assets_root.string());
                }
            }
        }

        if (m_services.editor_resources)
        {
            m_services.editor_resources->bootstrapImportOnce();
        }
        m_initialized = true;

        auto& ctx = m_editor_ui.context();
        ctx.enter_play_mode = [this]()
            {
                if (m_mode_callbacks.enter_play_mode)
                    m_mode_callbacks.enter_play_mode();
            };

        ctx.exit_play_mode = [this]()
            {
                if (m_mode_callbacks.exit_play_mode)
                    m_mode_callbacks.exit_play_mode();
            };

        ctx.is_play_mode = [this]() -> bool
            {
                return m_mode_callbacks.is_play_mode ? m_mode_callbacks.is_play_mode() : false;
            };
        ctx.open_scene = [this](const std::string& scene_vpath)
            {
                openSceneByVPath(scene_vpath);
            };
        ctx.save_scene = [this]() -> bool
            {
                return saveActiveScene();
            };
        ctx.save_scene_as = [this](const std::string& scene_vpath) -> bool
            {
                return saveActiveSceneAs(scene_vpath);
            };

        ctx.clearSceneDocument();
        ctx.setStatusMessage("");
        restoreStartupScene();
    }

    void EditorLayer::onDetach()
    {
        if (!m_initialized)
            return;

        // 先断开所有 EditorContext 回调，避免 shutdown 期间触发 use-after-free
        auto& ctx = m_editor_ui.context();
        ctx.notify_asset_source_event = {};
        ctx.open_scene = {};
        ctx.save_scene = {};
        ctx.save_scene_as = {};

        ctx.enter_play_mode = {};
        ctx.exit_play_mode = {};
        ctx.is_play_mode = {};

        // 还可以顺便把选择清空，避免其他地方读到野值
        ctx.selected = entt::null;
        ctx.active_scene = nullptr;

        if (m_services.editor_ext)
            m_services.editor_ext->has_editor_camera = false;

        m_editor_ui.shutdown();
        m_initialized = false;
    }

    bool EditorLayer::openSceneByVPath(const std::string& scene_vpath, OpenSceneFlags flags)
    {
        if (!m_services.scene || !m_services.render || !m_services.resources)
            return false;

        auto vfs = m_services.resources->getVFS();
        if (!vfs)
        {
            HBD_CORE_WARN("EditorLayer: open_scene failed, VFS is null");
            return false;
        }

        auto native = vfs->resolve(scene_vpath);
        if (!native)
        {
            HBD_CORE_WARN("EditorLayer: open_scene resolve failed: {}", scene_vpath);
            return false;
        }

        auto new_scene = std::make_shared<Scene>();
        if (!SceneSerializer::DeserializeFromFile(*new_scene, *native))
        {
            HBD_CORE_WARN("EditorLayer: open_scene deserialize failed: {}", native->string());
            return false;
        }

        new_scene->setCurrentSceneVPath(scene_vpath);
        new_scene->setName(native->stem().string());
        new_scene->setDirty(false);

        bool replaced = false;
        if (m_services.set_editor_scene)
        {
            replaced = m_services.set_editor_scene(new_scene);
        }
        else
        {
            m_services.scene->setActiveScene(new_scene);
            m_services.render->setScene(new_scene);
            replaced = (m_services.scene->getActiveScene() == new_scene);
        }

        if (!replaced || m_services.scene->getActiveScene() != new_scene)
        {
            HBD_CORE_WARN("EditorLayer: open_scene failed to replace active editor scene: {}", scene_vpath);
            return false;
        }

        m_editor_ui.setActiveScene(new_scene.get());
        m_editor_ui.context().active_scene = new_scene.get();
        if (flags.clear_selection)
            m_editor_ui.context().selected = entt::null;
        m_editor_ui.context().setSceneDocument(scene_vpath, native->string());
        m_editor_ui.context().setStatusMessage("");

        if (flags.remember_last_opened)
            saveLastOpenedScene(scene_vpath);

        HBD_CORE_INFO("EditorLayer: opened scene {}", scene_vpath);
        return true;
    }

    bool EditorLayer::saveActiveScene()
    {
        auto& ctx = m_editor_ui.context();
        if (ctx.current_scene_vpath.empty())
        {
            ctx.setStatusMessage("当前场景未保存，请使用 Save As");
            return false;
        }

        return saveActiveSceneAs(ctx.current_scene_vpath);
    }

    bool EditorLayer::saveActiveSceneAs(const std::string& scene_vpath)
    {
        auto active_scene = m_services.scene ? m_services.scene->getActiveScene() : nullptr;
        auto& ctx = m_editor_ui.context();
        if (!active_scene)
        {
            ctx.setStatusMessage("当前没有可保存的场景");
            return false;
        }

        if (!m_services.resources)
        {
            ctx.setStatusMessage("资源系统不可用，无法保存场景");
            return false;
        }

        std::string normalized_vpath;
        if (scene_vpath.empty())
        {
            std::filesystem::path initial_dir = m_assets_root.empty() ? std::filesystem::path{} : (m_assets_root / "Scenes");
            if (ctx.current_scene_native_path.empty())
            {
                if (!std::filesystem::exists(initial_dir))
                    initial_dir = m_assets_root;
            }
            else
            {
                initial_dir = std::filesystem::path(ctx.current_scene_native_path).parent_path();
            }

            std::string default_name = active_scene->getName().empty() ? std::string("Untitled") : active_scene->getName();
            std::filesystem::path default_file = std::filesystem::path(default_name).replace_extension(".scene");
            auto selected_path = ShowSaveSceneDialogWin32(
                m_services.window ? m_services.window->getNativeWindow() : nullptr,
                initial_dir,
                default_file.wstring());
            if (!selected_path)
                return false;

            if (!toAssetVPath(*selected_path, normalized_vpath))
            {
                ctx.setStatusMessage("保存路径必须位于 Assets 目录下");
                return false;
            }
        }
        else if (!normalizeSceneSaveVPath(scene_vpath, normalized_vpath))
        {
            ctx.setStatusMessage("Save As 路径无效，需使用 asset 相对路径");
            return false;
        }

        auto vfs = m_services.resources->getVFS();
        if (!vfs)
        {
            ctx.setStatusMessage("VFS 不可用，无法保存场景");
            return false;
        }

        auto native = vfs->resolveForWrite(normalized_vpath);
        if (!native)
        {
            ctx.setStatusMessage("无法解析保存路径");
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(native->parent_path(), ec);
        if (ec)
        {
            ctx.setStatusMessage("无法创建场景目录");
            HBD_CORE_WARN("EditorLayer: failed to create scene dir {} ({})",
                          native->parent_path().string(),
                          ec.message());
            return false;
        }

        const bool existed = std::filesystem::exists(*native);
        if (!SceneSerializer::SerializeToFile(*active_scene, *native))
        {
            ctx.setStatusMessage("场景保存失败");
            HBD_CORE_WARN("EditorLayer: save_scene serialize failed: {}", native->string());
            return false;
        }

        active_scene->setCurrentSceneVPath(normalized_vpath);
        active_scene->setName(native->stem().string());
        active_scene->setDirty(false);
        ctx.setSceneDocument(normalized_vpath, native->string());
        ctx.setStatusMessage(existed ? "场景已保存" : "场景已另存为");
        saveLastOpenedScene(normalized_vpath);

        if (m_services.editor_resources)
        {
            m_services.editor_resources->enqueueSourceChanged(
                normalized_vpath,
                existed ? AssetSourceChangeType::Modified : AssetSourceChangeType::Added);
        }

        HBD_CORE_INFO("EditorLayer: saved scene {}", normalized_vpath);
        return true;
    }

    void EditorLayer::restoreStartupScene()
    {
        const std::string last_scene = loadLastOpenedScene();
        if (!last_scene.empty() && openSceneByVPath(last_scene, OpenSceneFlags{ false, true }))
            return;

        if (tryOpenProjectDefaultScene())
            return;

        if (tryOpenScannedScene())
            return;

        createUntitledScene("未找到可用场景，已创建空场景");
    }

    bool EditorLayer::tryOpenProjectDefaultScene()
    {
        const std::filesystem::path settings_path = GetProjectSettingsPath();
        std::ifstream ifs(settings_path);
        if (!ifs.is_open())
            return false;

        json root;
        try
        {
            ifs >> root;
        }
        catch (const std::exception& e)
        {
            HBD_CORE_WARN("EditorLayer: failed to parse project settings {} ({})",
                          settings_path.string(),
                          e.what());
            return false;
        }

        const std::string default_scene = root.value("default_scene", std::string{});
        return !default_scene.empty() && openSceneByVPath(default_scene);
    }

    bool EditorLayer::tryOpenScannedScene()
    {
        if (m_assets_root.empty())
            return false;

        const std::filesystem::path scenes_dir = m_assets_root / "Scenes";
        if (!std::filesystem::exists(scenes_dir) || !std::filesystem::is_directory(scenes_dir))
            return false;

        struct Candidate
        {
            std::filesystem::path physical;
            std::filesystem::file_time_type modified{};
            std::string vpath;
        };

        std::vector<Candidate> candidates;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(scenes_dir, ec), end; it != end && !ec; it.increment(ec))
        {
            if (!it->is_regular_file())
                continue;

            const auto& physical = it->path();
            if (physical.extension() != ".scene")
                continue;

            std::string vpath;
            if (!toAssetVPath(physical, vpath))
                continue;

            candidates.push_back(Candidate{ physical, it->last_write_time(ec), vpath });
            ec.clear();
        }

        if (candidates.empty())
            return false;

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
        {
            if (a.modified != b.modified)
                return a.modified > b.modified;
            return a.vpath < b.vpath;
        });

        for (const Candidate& candidate : candidates)
        {
            if (openSceneByVPath(candidate.vpath))
                return true;
        }

        return false;
    }

    void EditorLayer::createUntitledScene(const char* reason)
    {
        auto scene = std::make_shared<Scene>();
        scene->setDirty(true);
        scene->setCurrentSceneVPath("");
        scene->setName("Untitled");

        bool replaced = false;
        if (m_services.set_editor_scene)
        {
            replaced = m_services.set_editor_scene(scene);
        }
        else if (m_services.scene && m_services.render)
        {
            m_services.scene->setActiveScene(scene);
            m_services.render->setScene(scene);
            replaced = (m_services.scene->getActiveScene() == scene);
        }

        if (!replaced)
        {
            HBD_CORE_WARN("EditorLayer: failed to create untitled scene");
            return;
        }

        auto& ctx = m_editor_ui.context();
        m_editor_ui.setActiveScene(scene.get());
        ctx.active_scene = scene.get();
        ctx.selected = entt::null;
        ctx.clearSceneDocument();
        scene->setDirty(true);
        ctx.scene_dirty = true;
        ctx.setStatusMessage(reason ? reason : "");
    }

    void EditorLayer::saveLastOpenedScene(const std::string& scene_vpath) const
    {
        const std::filesystem::path session_path = GetEditorUserSettingsPath();
        std::error_code ec;
        std::filesystem::create_directories(session_path.parent_path(), ec);
        if (ec)
        {
            HBD_CORE_WARN("EditorLayer: failed to create editor session dir {} ({})",
                          session_path.parent_path().string(),
                          ec.message());
            return;
        }

        json root;
        root["last_open_scene"] = scene_vpath;

        std::ofstream ofs(session_path, std::ios::trunc);
        if (!ofs.is_open())
        {
            HBD_CORE_WARN("EditorLayer: failed to write editor session {}", session_path.string());
            return;
        }

        ofs << root.dump(2);
    }

    std::string EditorLayer::loadLastOpenedScene() const
    {
        const std::filesystem::path session_path = GetEditorUserSettingsPath();
        std::ifstream ifs(session_path);
        if (!ifs.is_open())
            return {};

        json root;
        try
        {
            ifs >> root;
        }
        catch (const std::exception& e)
        {
            HBD_CORE_WARN("EditorLayer: failed to parse editor state {} ({})",
                          session_path.string(),
                          e.what());
            return {};
        }

        return root.value("last_open_scene", std::string{});
    }

    void EditorLayer::onUpdate(float dt)
    {
        if (!m_initialized)
            return;

        m_editor_ui.updateViewportState();
        updateEditorCamera(dt);
        pollFileWatcher();

        if (m_services.editor_resources)
        {
            m_services.editor_resources->processImportQueue(4, 2);
        }

        if (m_services.consume_pick_result)
        {
            uint32_t picked = 0;
            if (m_services.consume_pick_result(picked))
            {
                auto& ctx = m_editor_ui.context();
                ctx.selected = (picked == 0) ? entt::null : static_cast<entt::entity>(picked);
            }
        }

        // Pre-render sync: publish this frame's camera/render inputs.
        updateFrameContext();
    }

    void EditorLayer::onImGuiRender()
    {
        if (!m_initialized)
            return;

        auto active_scene = m_services.scene->getActiveScene();
        m_editor_ui.setActiveScene(active_scene.get());

        auto& ctx = m_editor_ui.context();
        ctx.active_scene = active_scene.get();
        ctx.gizmo_view = m_services.render->getLastView();
        ctx.gizmo_proj = m_services.render->getLastProj();

        m_editor_ui.drawPanels();
        m_editor_ui.drawViewports(m_services.render->getSceneColorTexture(),
                                  m_services.render->getGameColorTexture());
    }

    void EditorLayer::updateFrameContext()
    {
        auto* frame_context = m_services.frame_context;
        auto* render_flags = m_services.render_flags;
        auto* editor_ext = m_services.editor_ext;
        if (!frame_context || !render_flags || !editor_ext)
            return;

        auto& ctx = m_editor_ui.context();
        frame_context->viewport_size = {ctx.scene_viewport_size.x, ctx.scene_viewport_size.y};
        editor_ext->viewport_active = ctx.scene_viewport_image_hovered;
        editor_ext->render_scene_view = ctx.scene_viewport_size.x > 1.0f && ctx.scene_viewport_size.y > 1.0f;
        editor_ext->render_game_view = ctx.game_viewport_size.x > 1.0f && ctx.game_viewport_size.y > 1.0f;
        editor_ext->scene_viewport_size = {ctx.scene_viewport_size.x, ctx.scene_viewport_size.y};
        editor_ext->game_viewport_size = {ctx.game_viewport_size.x, ctx.game_viewport_size.y};
        editor_ext->selected_entity_id =
            (ctx.selected == entt::null) ? 0u : static_cast<uint32_t>(entt::to_integral(ctx.selected));
        editor_ext->pan_tool = ctx.pan_tool;
        if (editor_ext->render_scene_view)
        {
            editor_ext->has_editor_camera = true;
            editor_ext->editor_view = m_editor_camera.getView();
            editor_ext->editor_proj = m_editor_camera.getProjection();
            editor_ext->editor_camera_pos = m_editor_camera.getPosition();
        }
        else
        {
            editor_ext->has_editor_camera = false;
        }

        *render_flags = RenderFlags::Forward | RenderFlags::PickingID | RenderFlags::Grid | RenderFlags::Gizmos;
        if (editor_ext->selected_entity_id != 0)
        {
            *render_flags |= RenderFlags::SelectionOutline;
        }

        if (ctx.request_pick)
        {
            editor_ext->request_pick = true;
            editor_ext->pick_x = ctx.pick_x;
            editor_ext->pick_y = ctx.pick_y;
            ctx.request_pick = false;
        }
        else
        {
            editor_ext->request_pick = false;
        }
    }

    void EditorLayer::updateEditorCamera(float dt)
    {
        auto& ctx = m_editor_ui.context();
        ctx.suppress_tool_shortcuts = false;

        if (!m_services.input || !m_services.editor_ext)
            return;

        if (ctx.scene_viewport_size.x > 1.0f && ctx.scene_viewport_size.y > 1.0f)
        {
            m_editor_camera.setViewportSize(ctx.scene_viewport_size.x, ctx.scene_viewport_size.y);
        }

        const bool camera_input_active = ctx.scene_viewport_image_hovered;
        const InputState& input = m_services.input->getState();

        const float mdx = camera_input_active ? input.getMouseDeltaX() : 0.0f;
        const float mdy = camera_input_active ? input.getMouseDeltaY() : 0.0f;
        const float scroll_y = camera_input_active ? input.getScrollDeltaY() : 0.0f;

        const bool lmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const bool mmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_MIDDLE);
        const bool rmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);
        const bool mmb_for_camera = mmb_down || (ctx.pan_tool && lmb_down);

        const bool key_w = camera_input_active && input.isKeyDown(GLFW_KEY_W);
        const bool key_a = camera_input_active && input.isKeyDown(GLFW_KEY_A);
        const bool key_s = camera_input_active && input.isKeyDown(GLFW_KEY_S);
        const bool key_d = camera_input_active && input.isKeyDown(GLFW_KEY_D);
        const bool key_q = camera_input_active && input.isKeyDown(GLFW_KEY_Q);
        const bool key_e = camera_input_active && input.isKeyDown(GLFW_KEY_E);
        const bool key_shift = camera_input_active &&
            (input.isKeyDown(GLFW_KEY_LEFT_SHIFT) || input.isKeyDown(GLFW_KEY_RIGHT_SHIFT));
        const bool key_ctrl = camera_input_active &&
            (input.isKeyDown(GLFW_KEY_LEFT_CONTROL) || input.isKeyDown(GLFW_KEY_RIGHT_CONTROL));
        const bool key_alt = camera_input_active &&
            (input.isKeyDown(GLFW_KEY_LEFT_ALT) || input.isKeyDown(GLFW_KEY_RIGHT_ALT));

        ctx.suppress_tool_shortcuts = camera_input_active && rmb_down && !key_alt;

        m_editor_camera.update(dt,
                               camera_input_active,
                               mdx,
                               mdy,
                               scroll_y,
                               lmb_down,
                               mmb_for_camera,
                               rmb_down,
                               key_w,
                               key_a,
                               key_s,
                               key_d,
                               key_q,
                               key_e,
                               key_shift,
                               key_ctrl,
                               key_alt);
    }

    void EditorLayer::bindAssetChangeCallback()
    {
        if (!m_services.editor_resources)
            return;

        auto& ctx = m_editor_ui.context();
        ctx.notify_asset_source_event = [this](const AssetSourceEvent& event) {
            if (!m_services.editor_resources)
                return;

            switch (event.type)
            {
            case AssetSourceEventType::Added:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Added);
                break;
            case AssetSourceEventType::Modified:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Modified);
                break;
            case AssetSourceEventType::Removed:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Removed);
                break;
            case AssetSourceEventType::Moved:
                (void)m_services.editor_resources->moveAsset(event.old_path, event.new_path);
                break;
            default:
                break;
            }
        };
    }

    void EditorLayer::pollFileWatcher()
    {
        if (!m_services.editor_resources || !m_file_watcher.isInitialized())
            return;

        m_file_watcher.poll([this](const std::filesystem::path& physical_path, FileWatcherChangeType type) {
            std::string source_vpath;
            if (!toAssetVPath(physical_path, source_vpath))
                return;

            const AssetSourceChangeType change =
                (type == FileWatcherChangeType::Removed)
                    ? AssetSourceChangeType::Removed
                    : (type == FileWatcherChangeType::Added ? AssetSourceChangeType::Added
                                                            : AssetSourceChangeType::Modified);
            m_services.editor_resources->enqueueSourceChanged(source_vpath, change);
        });
    }

    bool EditorLayer::toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const
    {
        out_vpath.clear();
        if (m_assets_root.empty() || physical_path.empty())
            return false;

        std::error_code ec;
        auto rel = std::filesystem::relative(physical_path, m_assets_root, ec);
        if (ec || rel.empty())
            return false;

        std::string rel_str = rel.generic_string();
        while (!rel_str.empty() && (rel_str.front() == '/' || rel_str.front() == '\\'))
            rel_str.erase(rel_str.begin());
        if (rel_str.empty())
            return false;

        out_vpath = std::string("asset:") + rel_str;
        return true;
    }

    void EditorLayer::setModeCallbacks(EditorModeCallbacks callbacks)
    {
        m_mode_callbacks = std::move(callbacks);
    }
} // namespace Hybrid
