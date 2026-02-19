#pragma once
#include <cstdint>
#include <imgui.h>

namespace Hybrid
{
    class Scene; // 先前向声明，下一步再接入真实 Scene 接口

    struct EditorContext
    {
        // --- Data binding ---
        Scene* active_scene = nullptr;      // 当前编辑的场景（Edit 模式通常指 EditorScene）
        uint64_t selected_id = 0;           // 当前选中对象（先用 ID 占位；下一步换成 Entity/UUID）

        // --- Viewport state ---
        bool viewport_hovered = false;
        bool viewport_focused = false;

        ImVec2 viewport_size = ImVec2(1, 1); // 视口内容区大小（像素）
        ImVec2 viewport_min = ImVec2(0, 0); // 视口 Image 的屏幕坐标 min（用于 picking/gizmo）
        ImVec2 viewport_max = ImVec2(0, 0); // 视口 Image 的屏幕坐标 max

        // --- Camera mode ---
        bool use_game_camera = false;        // Game/Editor 相机模式切换
    };
} // namespace Hybrid
