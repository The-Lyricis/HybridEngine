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
            dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev) {
                if (!ev.isRepeat())
                    m_state.onKey(ev.getKeyCode(), true);
                return false;
                });
            dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& ev) {
                m_state.onKey(ev.getKeyCode(), false);
                return false;
                });
            dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& ev) {
                m_state.onMouseButton(ev.getMouseButton(), true);
                return false;
                });
            dispatcher.dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& ev) {
                m_state.onMouseButton(ev.getMouseButton(), false);
                return false;
                });
            dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent& ev) {
                m_state.onMouseMove(ev.GetX(), ev.GetY());
                return false;
                });
            dispatcher.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& ev) {
                m_state.onScroll(ev.GetXOffset(), ev.GetYOffset());
                return false;
                });
            dispatcher.dispatch<KeyTypedEvent>([this](KeyTypedEvent& ev) {
                m_state.onText(static_cast<char32_t>(ev.getKeyCode()));
                return false;
                });
        }

        const InputState& getState() const { return m_state; }

    private:
        InputState m_state;
    };
}
