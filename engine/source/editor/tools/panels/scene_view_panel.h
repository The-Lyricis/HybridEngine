#pragma once

#include "i_editor_panel.h"

#include <cstdint>

#include <entt/entt.hpp>

#include "editor/tools/panels/scene/scene_view_gizmo.h"
#include "editor/tools/panels/scene/scene_view_viewport.h"
#include "editor/services/render/editor_image_handle.h"

namespace Hybrid
{
    struct EditorContext;

    class SceneViewPanel final : public IEditorPanel
    {
    public:
        SceneViewPanel() : IEditorPanel(EditorPanelId::SceneView, "Scene") {}

        void setTexture(EditorImageHandle image) { m_image = image; }
        void updateViewportState(EditorContext& ctx);
        void onImGuiRender(EditorContext& ctx) override;

    private:
        EditorImageHandle m_image;
        SceneViewGizmoDragState m_gizmo_drag_state{};
        SceneViewViewportState m_viewport_state{};
    };
} // namespace Hybrid
