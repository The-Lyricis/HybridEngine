#include "layer.h"

#include <algorithm>
#include <stdexcept>

namespace Hybrid
{
    LayerStack::~LayerStack()
    {
        clear();
    }

    Layer& LayerStack::pushLayer(std::unique_ptr<Layer> layer)
    {
        if (!layer)
            throw std::invalid_argument("LayerStack::pushLayer requires a non-null layer");
        Layer& result = *layer;
        m_Layers.emplace(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), std::move(layer));
        ++m_LayerInsertIndex;
        return result;
    }

    Layer& LayerStack::pushOverlay(std::unique_ptr<Layer> overlay)
    {
        if (!overlay)
            throw std::invalid_argument("LayerStack::pushOverlay requires a non-null layer");
        Layer& result = *overlay;
        m_Layers.emplace_back(std::move(overlay));
        return result;
    }

    void LayerStack::popLayer(Layer* layer)
    {
        auto it = std::find_if(m_Layers.begin(),
                               m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex),
                               [layer](const std::unique_ptr<Layer>& item) { return item.get() == layer; });
        if (it != m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex))
        {
            if (*it)
                (*it)->onDetach();
            m_Layers.erase(it);
            --m_LayerInsertIndex;
        }
    }

    void LayerStack::popOverlay(Layer* overlay)
    {
        auto it = std::find_if(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex),
                               m_Layers.end(),
                               [overlay](const std::unique_ptr<Layer>& item) { return item.get() == overlay; });
        if (it != m_Layers.end())
        {
            if (*it)
                (*it)->onDetach();
            m_Layers.erase(it);
        }
    }

    void LayerStack::clear()
    {
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(m_LayerInsertIndex) - 1; i >= 0; --i)
        {
            Layer* layer = m_Layers[static_cast<size_t>(i)].get();
            if (layer)
                layer->onDetach();
        }

        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(m_Layers.size()) - 1;
             i >= static_cast<std::ptrdiff_t>(m_LayerInsertIndex);
             --i)
        {
            Layer* overlay = m_Layers[static_cast<size_t>(i)].get();
            if (overlay)
                overlay->onDetach();
        }

        m_Layers.clear();
        m_LayerInsertIndex = 0;
    }
} // namespace Hybrid
