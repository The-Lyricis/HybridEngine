#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <runtime/function/render/surface_io.h>

struct GLFWwindow; // 前置声明

namespace Hybrid
{
    //class SurfaceIO; // 你已有的 GLFW 桥接层（或按我之前给的 SurfaceIO）

    class InputSystem
    {
    public:
        static InputSystem& get()
        {
            static InputSystem s_instance;
            return s_instance;
        }

        // 初始化：注册回调。必须在创建 window / SurfaceIO 后调用一次
        void initialize(SurfaceIO& surface);

        // 每帧调用：清理 pressed/released 边沿、清理鼠标增量、更新 focus 状态等
        void tick();

        // ====== 键盘查询：支持全部 GLFW Key ======
        bool isKeyDown(int glfw_key) const;
        bool wasKeyPressed(int glfw_key) const;   // 本帧刚按下
        bool wasKeyReleased(int glfw_key) const;  // 本帧刚松开

        // ====== 鼠标查询：支持全部 GLFW Mouse Button ======
        bool isMouseDown(int glfw_button) const;
        bool wasMousePressed(int glfw_button) const;
        bool wasMouseReleased(int glfw_button) const;

        // ====== 鼠标移动/滚轮 ======
        double getMouseDeltaX() const { return m_mouse_delta_x; }
        double getMouseDeltaY() const { return m_mouse_delta_y; }
        double getScrollDeltaX() const { return m_scroll_delta_x; }
        double getScrollDeltaY() const { return m_scroll_delta_y; }

        // ====== 字符输入（文本输入）=====
        // 用于 UI/控制台输入等（GLFW char callback）
        // 每帧收集，tick() 后清空
        const std::u32string& getTextInputBuffer() const { return m_text_input; }

        // ====== 状态：是否接受输入 ======
        // 例如鼠标未锁定 / 窗口失焦时可以认为输入无效
        bool isInputValid() const { return m_input_valid; }

    private:
        InputSystem() = default;

        // GLFW 回调入口（由 SurfaceIO 调用）
        void onKey(int key, int scancode, int action, int mods);
        void onMouseButton(int button, int action, int mods);
        void onCursorPos(double x, double y);
        void onScroll(double xoffset, double yoffset);
        void onChar(unsigned int codepoint);

        // 安全检查
        static bool isKeyInRange(int key);
        static bool isMouseButtonInRange(int btn);

    private:
        struct ButtonState
        {
            bool down = false;
            bool pressed = false; // edge: down this frame
            bool released = false; // edge: up this frame
        };

        SurfaceIO* m_surface{ nullptr };

        // GLFW 官方定义：GLFW_KEY_LAST, GLFW_MOUSE_BUTTON_LAST
        static constexpr int kMaxKeys = 348; // 兼容：GLFW_KEY_LAST通常为348（建议仍做范围判断）
        static constexpr int kMaxMouse = 7;   // GLFW_MOUSE_BUTTON_LAST通常为7

        // 用 array 便于 cache-friendly 与 O(1) 查询
        std::array<ButtonState, kMaxKeys + 1>  m_keys{};
        std::array<ButtonState, kMaxMouse + 1> m_mouse{};

        // 鼠标位置与增量
        bool   m_mouse_inited{ false };
        double m_last_mouse_x{ 0.0 };
        double m_last_mouse_y{ 0.0 };
        double m_mouse_delta_x{ 0.0 };
        double m_mouse_delta_y{ 0.0 };

        // 滚轮增量（本帧）
        double m_scroll_delta_x{ 0.0 };
        double m_scroll_delta_y{ 0.0 };

        // 文本输入（本帧）
        std::u32string m_text_input;

        // 输入是否有效
        bool m_input_valid{ true };
    };
} // namespace Pilot
