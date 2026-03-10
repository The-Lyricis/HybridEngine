#pragma once
#include "i_editor_panel.h"
#include <cstdint>

namespace Hybrid
{
    enum class ViewportPanelMode
    {
        Scene = 0,
        Game
    };

    class ViewportPanel final : public IEditorPanel
    {
    public:
        explicit ViewportPanel(ViewportPanelMode mode) : m_mode(mode) {}

        const char* getName() const override { return m_mode == ViewportPanelMode::Scene ? "Scene" : "Game"; }

        void setTexture(uint32_t colorTex) { m_colorTextureID = colorTex; }
        void updateViewportState(EditorContext& ctx);
        void onImGuiRender(EditorContext& ctx) override;

    private:
        ViewportPanelMode m_mode = ViewportPanelMode::Scene;
        uint32_t m_colorTextureID = 0;
    };
}
