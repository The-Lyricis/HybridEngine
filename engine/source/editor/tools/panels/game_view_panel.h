#pragma once

#include "i_editor_panel.h"

#include <cstdint>
#include <filesystem>

#include "editor/services/render/editor_image_handle.h"

namespace Hybrid
{
    struct EditorContext;

    class GameViewPanel final : public IEditorPanel
    {
    public:
        GameViewPanel() : IEditorPanel(EditorPanelId::GameView, "Game") {}

        void setTexture(EditorImageHandle image) { m_image = image; }
        void updateViewportState(EditorContext& ctx);
        void onImGuiRender(EditorContext& ctx) override;

    private:
        void loadSettings(EditorContext& ctx);
        void saveSettings(const EditorContext& ctx);
        EditorImageHandle m_image;
        bool m_missingTextureLogged = false;
        std::filesystem::path m_loaded_project;
    };
} // namespace Hybrid
