#pragma once

#include <imgui.h>          // ImVec2 需要这个
#include <glm/mat4x4.hpp>
#include <entt/entt.hpp>

namespace Hybrid
{
    class Scene;            // 前向声明：只用指针足够

    struct EditorContext
    {
        Scene* active_scene = nullptr;

        // 选中实体（如果你用 entt::entity）
        entt::entity selected = entt::null;

        // Viewport 状态
        bool  viewport_hovered = false;
        bool  viewport_focused = false;
        ImVec2 viewport_size = { 1, 1 };
        ImVec2 viewport_min = { 0, 0 };
        ImVec2 viewport_max = { 0, 0 };

        // Picking
        bool request_pick = false;
        int  pick_x = 0;
        int  pick_y = 0;

        // ImGuizmo 需要的矩阵（与当前渲染相机一致）
        glm::mat4 gizmo_view = glm::mat4(1.0f);
        glm::mat4 gizmo_proj = glm::mat4(1.0f);

        // 输入仲裁：gizmo 拖动时不要让相机响应
        bool gizmo_using = false;
        bool use_game_camera = false;
        bool viewport_image_hovered = false;

        bool pan_tool = false;
    };
} // namespace Hybrid
