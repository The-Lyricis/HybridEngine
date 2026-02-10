#pragma once
#include "runtime/core/event/layer.h"
#include "runtime/core/event/input_event.h"
#include "runtime/function/input/input_state.h"

namespace Hybrid
{
    class InputLayer : public Layer
    {
    public:
        InputLayer() : Layer("InputLayer") {}

        void onUpdate(float /*dt*/) override
        {
            m_state.newFrame();
        }

        void onEvent(Event& e) override
        {
            EventDispatcher dispatcher(e);
            dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev) {
                if (!ev.IsRepeat())
                    m_state.onKey(ev.GetKeyCode(), true);
                return false;
                });
            dispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& ev) {
                m_state.onKey(ev.GetKeyCode(), false);
                return false;
                });
            dispatcher.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& ev) {
                m_state.onMouseButton(ev.GetMouseButton(), true);
                return false;
                });
            dispatcher.Dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& ev) {
                m_state.onMouseButton(ev.GetMouseButton(), false);
                return false;
                });
            dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& ev) {
                m_state.onMouseMove(ev.GetX(), ev.GetY());
                return false;
                });
            dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& ev) {
                m_state.onScroll(ev.GetXOffset(), ev.GetYOffset());
                return false;
                });
            dispatcher.Dispatch<KeyTypedEvent>([this](KeyTypedEvent& ev) {
                m_state.onText(static_cast<char32_t>(ev.GetKeyCode()));
                return false;
                });
        }

        const InputState& getState() const { return m_state; }

    private:
        InputState m_state;
    };
}
