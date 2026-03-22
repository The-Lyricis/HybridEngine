#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace Hybrid
{
    class WindowSystem;
    class RenderSystem;
    class SceneManager;
    class Scene;
    class RuntimeResourceSystem;
    class EditorResourceSystem;
    class IEditorPlatformServices;
    class InputLayer;
    struct FrameContext;
    enum class RenderFlags : uint32_t;
    struct EditorRenderExt;

    // Service locator-style bundle injected into editor layers.
    struct EngineServices
    {
        WindowSystem* window = nullptr;                  // Native window and platform IO.
        RenderSystem* render = nullptr;                  // Render system API.
        SceneManager* scene = nullptr;                   // Scene lifecycle/selection source.
        RuntimeResourceSystem* resources = nullptr;      // Runtime asset services.
        EditorResourceSystem* editor_resources = nullptr;// Editor import/persistence services.
        IEditorPlatformServices* platform = nullptr;     // Editor platform-dependent dialogs and shell actions.
        InputLayer* input = nullptr;                     // Per-frame input state.
        FrameContext* frame_context = nullptr;           // Shared frame payload for renderer.
        RenderFlags* render_flags = nullptr;             // Per-frame pass mask.
        EditorRenderExt* editor_ext = nullptr;           // Editor-only render extension payload.
        std::function<bool(uint32_t&)> consume_pick_result; // Callback to read pick result from engine.
        std::function<bool(std::shared_ptr<Scene>)> set_editor_scene; // Replace the authoritative editor scene.
    };
} // namespace Hybrid
