#include "layer.h"

#include <algorithm>

namespace Hybrid
{
    LayerStack::~LayerStack()
    {
        clear();
    }

    void LayerStack::pushLayer(Layer* layer)
    {
        m_Layers.emplace(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), layer);
        ++m_LayerInsertIndex;
    }

    void LayerStack::pushOverlay(Layer* overlay)
    {
        m_Layers.emplace_back(overlay);
    }

    void LayerStack::popLayer(Layer* layer)
    {
        auto it = std::find(m_Layers.begin(), m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), layer);
        if (it != m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex))
        {
            if (*it)
            {
                (*it)->onDetach();
                delete *it;
            }
            m_Layers.erase(it);
            --m_LayerInsertIndex;
        }
    }

    void LayerStack::popOverlay(Layer* overlay)
    {
        auto it = std::find(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), m_Layers.end(), overlay);
        if (it != m_Layers.end())
        {
            if (*it)
            {
                (*it)->onDetach();
                delete *it;
            }
            m_Layers.erase(it);
        }
    }

    void LayerStack::clear()
    {
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(m_LayerInsertIndex) - 1; i >= 0; --i)
        {
            Layer* layer = m_Layers[static_cast<size_t>(i)];
            if (layer)
            {
                layer->onDetach();
                delete layer;
            }
        }

        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(m_Layers.size()) - 1;
             i >= static_cast<std::ptrdiff_t>(m_LayerInsertIndex);
             --i)
        {
            Layer* overlay = m_Layers[static_cast<size_t>(i)];
            if (overlay)
            {
                overlay->onDetach();
                delete overlay;
            }
        }

        m_Layers.clear();
        m_LayerInsertIndex = 0;
    }
} // namespace Hybrid
