#pragma once
#include <array>

namespace Hybrid
{
    class InputState
    {
    public:
        static constexpr int kMaxKeys = 348;
        static constexpr int kMaxMouse = 7;

        void NewFrame()
        {
            for (auto& k : m_keys) { k.pressed = false; k.released = false; }
            for (auto& b : m_mouse) { b.pressed = false; b.released = false; }
            m_mouse_delta_x = 0.0f;
            m_mouse_delta_y = 0.0f;
            m_scroll_delta_x = 0.0f;
            m_scroll_delta_y = 0.0f;
            m_text_input.clear();
        }

        void OnKey(int key, bool isDown)
        {
            if (key < 0 || key > kMaxKeys) return;
            auto& st = m_keys[key];
            if (isDown)
            {
                if (!st.down) st.pressed = true;
                st.down = true;
            }
            else
            {
                if (st.down) st.released = true;
                st.down = false;
            }
        }

        void OnMouseButton(int button, bool isDown)
        {
            if (button < 0 || button > kMaxMouse) return;
            auto& st = m_mouse[button];
            if (isDown)
            {
                if (!st.down) st.pressed = true;
                st.down = true;
            }
            else
            {
                if (st.down) st.released = true;
                st.down = false;
            }
        }

        void OnMouseMove(float x, float y)
        {
            if (!m_mouse_inited)
            {
                m_last_mouse_x = x; m_last_mouse_y = y;
                m_mouse_inited = true;
                return;
            }
            m_mouse_delta_x += (x - m_last_mouse_x);
            m_mouse_delta_y += (y - m_last_mouse_y);
            m_last_mouse_x = x;
            m_last_mouse_y = y;
        }

        void OnScroll(float xoff, float yoff)
        {
            m_scroll_delta_x += xoff;
            m_scroll_delta_y += yoff;
        }

        void OnText(char32_t c)
        {
            m_text_input.push_back(c);
        }

        // 查询
        bool IsKeyDown(int key) const { return InRangeKey(key) ? m_keys[key].down : false; }
        bool WasKeyPressed(int key) const { return InRangeKey(key) ? m_keys[key].pressed : false; }
        bool WasKeyReleased(int key) const { return InRangeKey(key) ? m_keys[key].released : false; }

        bool IsMouseDown(int btn) const { return InRangeMouse(btn) ? m_mouse[btn].down : false; }
        bool WasMousePressed(int btn) const { return InRangeMouse(btn) ? m_mouse[btn].pressed : false; }
        bool WasMouseReleased(int btn) const { return InRangeMouse(btn) ? m_mouse[btn].released : false; }

        float GetMouseDeltaX() const { return m_mouse_delta_x; }
        float GetMouseDeltaY() const { return m_mouse_delta_y; }
        float GetScrollDeltaX() const { return m_scroll_delta_x; }
        float GetScrollDeltaY() const { return m_scroll_delta_y; }
        const std::u32string& GetTextInput() const { return m_text_input; }

    private:
        struct ButtonState { bool down = false; bool pressed = false; bool released = false; };

        bool InRangeKey(int k) const { return k >= 0 && k <= kMaxKeys; }
        bool InRangeMouse(int b) const { return b >= 0 && b <= kMaxMouse; }

        std::array<ButtonState, kMaxKeys + 1> m_keys{};
        std::array<ButtonState, kMaxMouse + 1> m_mouse{};

        bool  m_mouse_inited = false;
        float m_last_mouse_x = 0.f;
        float m_last_mouse_y = 0.f;
        float m_mouse_delta_x = 0.f;
        float m_mouse_delta_y = 0.f;

        float m_scroll_delta_x = 0.f;
        float m_scroll_delta_y = 0.f;

        std::u32string m_text_input;
    };
}
