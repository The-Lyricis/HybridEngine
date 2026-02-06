#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <type_traits>
#include <runtime/function/render/surface_io.h>

#include "runtime/core/base/singleton.h" // 你的 Singleton<T> 所在头文件路径：请按工程实际调整

struct GLFWwindow; // 前置声明

namespace Hybrid
{
    struct LastKeyEvent
    {
        int key = -1;
        int scancode = 0;
        int mods = 0;
        double time_sec = 0.0;
        bool valid = false;
    };

    class InputSystem : public Singleton<InputSystem>
    {
        // 允许 Singleton<InputSystem>::getInstance() 访问私有构造
        friend class Singleton<InputSystem>;

    public:

        // 初始化：注册回调。必须在创建 window / SurfaceIO 后调用一次
        void initialize(SurfaceIO& surface);

        // 每帧调用：清理 pressed/released 边沿、清理鼠标增量、更新 focus 状态等
        void tick();

        // ====== 键盘查询：支持全部 GLFW Key ======
        bool isKeyDown(int glfw_key) const;
        bool wasKeyPressed(int glfw_key) const;
        bool wasKeyReleased(int glfw_key) const;

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
        const std::u32string& getTextInputBuffer() const { return m_text_input; }

        // ====== 状态：是否接受输入 ======
        bool isInputValid() const { return m_input_valid; }

        // ====== 最后一次按键 ======
        const LastKeyEvent& getLastKeyEvent() const { return m_last_key_event; }
        std::string getLastKeyName() const;

    private:
        // 重要：构造函数私有化，禁止外部构造，只能通过 Singleton 创建
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
            bool pressed = false;
            bool released = false;
        };

        SurfaceIO* m_surface{ nullptr };

        // GLFW 官方定义：GLFW_KEY_LAST, GLFW_MOUSE_BUTTON_LAST
        static constexpr int kMaxKeys = 348;
        static constexpr int kMaxMouse = 7;

        std::array<ButtonState, kMaxKeys + 1>  m_keys{};
        std::array<ButtonState, kMaxMouse + 1> m_mouse{};

        bool   m_mouse_inited{ false };
        double m_last_mouse_x{ 0.0 };
        double m_last_mouse_y{ 0.0 };
        double m_mouse_delta_x{ 0.0 };
        double m_mouse_delta_y{ 0.0 };

        double m_scroll_delta_x{ 0.0 };
        double m_scroll_delta_y{ 0.0 };

        std::u32string m_text_input;
        bool m_input_valid{ true };

        LastKeyEvent m_last_key_event;
    };
} // namespace Hybrid
