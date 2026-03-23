#pragma once

#include "i_editor_panel.h"

#include <cstdint>

namespace Hybrid
{
    struct EditorContext;

    class GameViewPanel final : public IEditorPanel
    {
    public:
        GameViewPanel() : IEditorPanel(EditorPanelId::GameView, "Game") {}

        void setTexture(uint32_t colorTex) { m_colorTextureID = colorTex; }
        void updateViewportState(EditorContext& ctx);
        void onImGuiRender(EditorContext& ctx) override;

    private:
        uint32_t m_colorTextureID = 0;
        bool m_missingTextureLogged = false;
    };
} // namespace Hybrid
