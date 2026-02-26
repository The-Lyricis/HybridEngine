#include "editor_layer.h"

#include <entt/entity/entity.hpp>
#include <system_error>
#include <utility>

#include "editor/core/editor_context.h"
#include "editor/services/asset/editor_resource_system.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/asset/runtime_resource_system.h"
#include "runtime/function/render/editor_render_ext.h"
#include "runtime/function/render/frame_context.h"
#include "runtime/function/render/render_flags.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/scene_manager.h"
#include "runtime/function/window/window_system.h"

namespace Hybrid
{
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
    }

    void EditorLayer::onDetach()
    {
        if (!m_initialized)
            return;

        m_editor_ui.context().notify_asset_source_event = {};
        m_editor_ui.shutdown();
        m_initialized = false;
    }

    void EditorLayer::onUpdate(float /*dt*/)
    {
        if (!m_initialized)
            return;

        pollFileWatcher();

        if (m_services.editor_resources)
        {
            m_services.editor_resources->processImportQueue(4, 2);
        }

        if (!m_services.consume_pick_result)
            return;

        uint32_t picked = 0;
        if (!m_services.consume_pick_result(picked))
            return;

        auto& ctx = m_editor_ui.context();
        ctx.selected = (picked == 0) ? entt::null : static_cast<entt::entity>(picked);
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
        m_editor_ui.drawViewport(m_services.render->getSceneColorTexture());

        updateFrameContext();
    }

    void EditorLayer::updateFrameContext()
    {
        auto* frame_context = m_services.frame_context;
        auto* render_flags = m_services.render_flags;
        auto* editor_ext = m_services.editor_ext;
        if (!frame_context || !render_flags || !editor_ext)
            return;

        auto& ctx = m_editor_ui.context();
        frame_context->viewport_size = {ctx.viewport_size.x, ctx.viewport_size.y};
        editor_ext->viewport_active = ctx.viewport_image_hovered;
        editor_ext->use_game_camera = ctx.use_game_camera;
        editor_ext->selected_entity_id =
            (ctx.selected == entt::null) ? 0u : static_cast<uint32_t>(entt::to_integral(ctx.selected));
        editor_ext->pan_tool = ctx.pan_tool;

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
} // namespace Hybrid

