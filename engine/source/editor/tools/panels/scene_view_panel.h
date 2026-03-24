#pragma once

#include "i_editor_panel.h"

#include <cstdint>

#include <entt/entt.hpp>

#include "editor/core/transform_snapshot.h"

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
        bool m_missingTextureLogged = false;
        bool m_gizmo_drag_active = false;
        entt::entity m_gizmo_drag_entity = entt::null;
        TransformSnapshot m_gizmo_drag_before{};
    };
} // namespace Hybrid
