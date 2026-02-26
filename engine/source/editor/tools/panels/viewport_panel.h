#pragma once
#include "i_editor_panel.h"
#include <cstdint>

namespace Hybrid
{
    class ViewportPanel final : public IEditorPanel
    {
    public:
        const char* getName() const override { return "Viewport"; }

        void setTexture(uint32_t colorTex) { m_colorTextureID = colorTex; }
        void updateViewportState(EditorContext& ctx);
        void onImGuiRender(EditorContext& ctx) override;

    private:
        uint32_t m_colorTextureID = 0;
    };
}
