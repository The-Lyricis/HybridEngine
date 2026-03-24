#pragma once

#include "i_editor_panel.h"

#include <cstdint>

#include <entt/entt.hpp>

#include "editor/tools/panels/scene/scene_view_gizmo.h"
#include "editor/tools/panels/scene/scene_view_viewport.h"

namespace Hybrid
{
    struct EditorContext;

    class SceneViewPanel final : public IEditorPanel
    {
    public:
        SceneViewPanel() : IEditorPanel(EditorPanelId::SceneView, "Scene") {}

        void setTexture(uint32_t colorTex) { m_colorTextureID = colorTex; }
        void updateViewportState(EditorContext& ctx);
        void onImGuiRender(EditorContext& ctx) override;

    private:
        uint32_t m_colorTextureID = 0;
        SceneViewGizmoDragState m_gizmo_drag_state{};
        SceneViewViewportState m_viewport_state{};
    };
} // namespace Hybrid
