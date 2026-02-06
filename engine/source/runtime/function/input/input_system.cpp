#include "runtime/function/input/input_system.h"
#include "runtime/function/render/surface_io.h"

#include <GLFW/glfw3.h>
#include <runtime/core/log/log_system.h>
#include <iostream>

namespace Hybrid
{
    // ===== Range check =====
    bool InputSystem::isKeyInRange(int key)
    {
        return key >= 0 && key <= kMaxKeys;
    }
    bool InputSystem::isMouseButtonInRange(int btn)
    {
        return btn >= 0 && btn <= kMaxMouse;
    }

    void InputSystem::initialize(SurfaceIO& surface)
    {
        m_surface = &surface;

        // 注册回调（你 SurfaceIO 需要提供对应注册接口）
        surface.registerOnKeyFunc([this](int key, int scancode, int action, int mods) {
            this->onKey(key, scancode, action, mods);
            });

        surface.registerOnMouseButtonFunc([this](int button, int action, int mods) {
            this->onMouseButton(button, action, mods);
            });

        surface.registerOnCursorPosFunc([this](double x, double y) {
            this->onCursorPos(x, y);
            });

        //// 如果你的 SurfaceIO 还没有滚轮/字符回调接口，建议补上：
        //// registerOnScrollFunc / registerOnCharFunc
        //// 这里假设你已经补齐，或你可以把它们放到 GLFW 原生 callback 中
        //if constexpr (true)
        //{
        //    surface.registerOnScrollFunc([this](double xoff, double yoff) {
        //        this->onScroll(xoff, yoff);
        //        });
        //    surface.registerOnCharFunc([this](unsigned int codepoint) {
        //        this->onChar(codepoint);
        //        });
        //}
    }

    void InputSystem::tick()
    {
        // 1) 更新输入有效性（策略可按你引擎逻辑调整）
        // 示例：focus mode 才有效
        if (m_surface)
            m_input_valid = m_surface->isFocusMode();
        else
            m_input_valid = true;

        // 2) 清理本帧边沿（pressed/released 只保留一帧）
        for (auto& k : m_keys)
        {
            k.pressed = false;
            k.released = false;
        }
        for (auto& b : m_mouse)
        {
            b.pressed = false;
            b.released = false;
        }

        // 3) 清理鼠标增量与滚轮增量、文本输入（全部是“本帧量”）
        m_mouse_delta_x = 0.0;
        m_mouse_delta_y = 0.0;
        m_scroll_delta_x = 0.0;
        m_scroll_delta_y = 0.0;
        m_text_input.clear();
    }

    // ===== Query =====
    bool InputSystem::isKeyDown(int glfw_key) const
    {
        if (!isKeyInRange(glfw_key)) return false;
        return m_keys[glfw_key].down;
    }
    bool InputSystem::wasKeyPressed(int glfw_key) const
    {
        if (!isKeyInRange(glfw_key)) return false;
        return m_keys[glfw_key].pressed;
    }
    bool InputSystem::wasKeyReleased(int glfw_key) const
    {
        if (!isKeyInRange(glfw_key)) return false;
        return m_keys[glfw_key].released;
    }

    bool InputSystem::isMouseDown(int glfw_button) const
    {
        if (!isMouseButtonInRange(glfw_button)) return false;
        return m_mouse[glfw_button].down;
    }
    bool InputSystem::wasMousePressed(int glfw_button) const
    {
        if (!isMouseButtonInRange(glfw_button)) return false;
        return m_mouse[glfw_button].pressed;
    }
    bool InputSystem::wasMouseReleased(int glfw_button) const
    {
        if (!isMouseButtonInRange(glfw_button)) return false;
        return m_mouse[glfw_button].released;
    }

    // ===== Callbacks =====
    void InputSystem::onKey(int key, int scancode, int action, int mods)
    {
        if (!isKeyInRange(key)) return;

		std::cout <<  "Key event: key=" << key << ", scancode=" << scancode << ", action=" << action << ", mods=" << mods << std::endl;

        auto& st = m_keys[key];

        if (action == GLFW_PRESS)
        {
            // 如果之前不是 down，则这是边沿 pressed
            if (!st.down)
                st.pressed = true;

            st.down = true;
        }
        else if (action == GLFW_RELEASE)
        {
            if (st.down)
                st.released = true;

            st.down = false;
        }
        else if (action == GLFW_REPEAT)
        {
            // repeat 一般不额外设置 pressed
            st.down = true;
        }
    }

    void InputSystem::onMouseButton(int button, int action, int /*mods*/)
    {
        if (!isMouseButtonInRange(button)) return;

        auto& st = m_mouse[button];

        if (action == GLFW_PRESS)
        {
            if (!st.down)
                st.pressed = true;
            st.down = true;
        }
        else if (action == GLFW_RELEASE)
        {
            if (st.down)
                st.released = true;
            st.down = false;
        }
    }

    void InputSystem::onCursorPos(double x, double y)
    {
        if (!m_surface) return;

        // 若不在 focus mode，则不累计相对移动，且重置 init
        if (!m_surface->isFocusMode())
        {
            m_mouse_inited = false;
            return;
        }

        if (!m_mouse_inited)
        {
            m_last_mouse_x = x;
            m_last_mouse_y = y;
            m_mouse_inited = true;
            return;
        }

        // 累计本帧相对移动
        m_mouse_delta_x += (x - m_last_mouse_x);
        m_mouse_delta_y += (y - m_last_mouse_y);

        m_last_mouse_x = x;
        m_last_mouse_y = y;
    }

    //void InputSystem::onScroll(double xoffset, double yoffset)
    //{
    //    m_scroll_delta_x += xoffset;
    //    m_scroll_delta_y += yoffset;
    //}

    //void InputSystem::onChar(unsigned int codepoint)
    //{
    //    // GLFW char callback 给的是 Unicode codepoint（UTF-32）
    //    m_text_input.push_back(static_cast<char32_t>(codepoint));
    //}

} // namespace Pilot
