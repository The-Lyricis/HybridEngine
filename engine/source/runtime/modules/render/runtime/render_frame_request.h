#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "runtime/modules/render/runtime/render_flags.h"

namespace Hybrid
{
    using RenderViewId = uint64_t;
    inline constexpr RenderViewId kInvalidRenderViewId = 0;
    class Scene;
    class InputState;

    inline constexpr uint32_t kInvalidEntityID = std::numeric_limits<uint32_t>::max();

    struct RenderSelectionState
    {
        std::vector<uint32_t> selected_entities;
        uint32_t active_entity = kInvalidEntityID;
        uint32_t hovered_entity = kInvalidEntityID;
    };

    struct RenderPostProcessState
    {
        bool enabled = false;
        bool enable_tone_mapping = false;
        bool enable_gamma_correction = false;
        float exposure = 1.0f;
        float gamma = 2.2f;
    };

    enum class RenderCameraSource : uint8_t
    {
        PrimarySceneCamera,
        ExplicitMatrices,
    };

    enum class RenderViewKind : uint8_t
    {
        Scene,
        Game,
    };

    struct RenderPickingRequest
    {
        int x = 0;
        int y = 0;
    };

    struct RenderViewRequest
    {
        std::string name;
        RenderViewKind kind = RenderViewKind::Game;
        glm::vec2 size{0.0f};
        RenderFlags flags = RenderFlags::Scene | RenderFlags::Shadow;
        RenderCameraSource camera_source = RenderCameraSource::PrimarySceneCamera;
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::vec3 camera_position{0.0f, 0.0f, 3.0f};
        RenderSelectionState selection;
        RenderPostProcessState post_process;
        bool viewport_active = true;
        bool select_tool = false;
        bool show_collider_debug = false;
        bool show_shadow_debug = false;
        std::optional<RenderPickingRequest> picking;
        RenderViewId id = kInvalidRenderViewId;
    };

    struct RenderFrameRequest
    {
        std::shared_ptr<Scene> scene;
        float dt = 0.0f;
        void* window_handle = nullptr;
        const InputState* input = nullptr;
        std::vector<RenderViewRequest> views;
    };

    struct RenderViewResult
    {
        std::string name;
        uint32_t color_texture = 0;
        std::optional<uint32_t> picked_entity;
        RenderViewId id = kInvalidRenderViewId;
    };

    struct RenderFrameResult
    {
        std::vector<RenderViewResult> views;
    };
} // namespace Hybrid
