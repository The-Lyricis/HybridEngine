#pragma once

#include "event.h"

#include <string>
#include <vector>

namespace Hybrid
{
    class Layer
    {
    public:
        explicit Layer(const std::string& name = "Layer") : m_Name(name) {}
        virtual ~Layer() = default;

        virtual void onBeginFrame() {}
        virtual void onAttach() {}
        virtual void onDetach() {}
        virtual void onUpdate(float dt) {}
        virtual void onEvent(Event& e) {}
        virtual void onImGuiRender() {}
        virtual void onEndFrame() {}

        const std::string& getName() const { return m_Name; }

    private:
        std::string m_Name;
    };

    class LayerStack
    {
    public:
        ~LayerStack();

        void pushLayer(Layer* layer);
        void pushOverlay(Layer* overlay);
        void popLayer(Layer* layer);
        void popOverlay(Layer* overlay);
        void clear();

        auto begin() { return m_Layers.begin(); }
        auto end() { return m_Layers.end(); }
        auto rbegin() { return m_Layers.rbegin(); }
        auto rend() { return m_Layers.rend(); }

    private:
        std::vector<Layer*> m_Layers;
        size_t m_LayerInsertIndex = 0; // insertion point for non-overlay layers
    };

} // namespace Hybrid
