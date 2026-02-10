#include "layer.h"

#include <algorithm>

namespace Hybrid
{
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
            m_Layers.erase(it);
            --m_LayerInsertIndex;
        }
    }

    void LayerStack::popOverlay(Layer* overlay)
    {
        auto it = std::find(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), m_Layers.end(), overlay);
        if (it != m_Layers.end())
        {
            m_Layers.erase(it);
        }
    }
} // namespace Hybrid
