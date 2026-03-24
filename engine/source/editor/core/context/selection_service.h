#pragma once

#include <vector>

#include <entt/entt.hpp>

namespace Hybrid
{
    struct EditorSelectionModel
    {
        std::vector<entt::entity> items;
        entt::entity active = entt::null;
        entt::entity range_anchor = entt::null;

        void clear()
        {
            items.clear();
            active = entt::null;
            range_anchor = entt::null;
        }

        bool empty() const
        {
            return items.empty();
        }

        size_t size() const
        {
            return items.size();
        }

        bool contains(entt::entity entity) const
        {
            for (entt::entity item : items)
            {
                if (item == entity)
                    return true;
            }
            return false;
        }

        void setSingle(entt::entity entity)
        {
            if (entity == entt::null)
            {
                clear();
                return;
            }

            items.clear();
            items.push_back(entity);
            active = entity;
            range_anchor = entity;
        }

        void add(entt::entity entity)
        {
            if (entity == entt::null || contains(entity))
                return;
            items.push_back(entity);
            active = entity;
            range_anchor = entity;
        }

        void remove(entt::entity entity)
        {
            if (entity == entt::null)
                return;

            for (auto it = items.begin(); it != items.end(); ++it)
            {
                if (*it != entity)
                    continue;

                items.erase(it);
                if (active == entity)
                    active = entt::null;
                if (range_anchor == entity)
                    range_anchor = entt::null;
                return;
            }
        }

        void toggle(entt::entity entity)
        {
            if (entity == entt::null)
                return;

            if (contains(entity))
            {
                remove(entity);
                return;
            }

            add(entity);
        }
    };

    class SelectionService
    {
    public:
        void clear()
        {
            m_state.clear();
        }

        bool empty() const
        {
            return m_state.empty();
        }

        size_t size() const
        {
            return m_state.size();
        }

        bool contains(entt::entity entity) const
        {
            return m_state.contains(entity);
        }

        void setSingle(entt::entity entity)
        {
            m_state.setSingle(entity);
        }

        void add(entt::entity entity)
        {
            m_state.add(entity);
        }

        void remove(entt::entity entity)
        {
            m_state.remove(entity);
        }

        void toggle(entt::entity entity)
        {
            m_state.toggle(entity);
        }

        const EditorSelectionModel& state() const
        {
            return m_state;
        }

        EditorSelectionModel& mutableState()
        {
            return m_state;
        }

        void replaceState(const EditorSelectionModel& state)
        {
            m_state = state;
        }

        const std::vector<entt::entity>& items() const
        {
            return m_state.items;
        }

        std::vector<entt::entity>& items()
        {
            return m_state.items;
        }

        entt::entity active() const
        {
            return m_state.active;
        }

        void setActive(entt::entity entity)
        {
            m_state.active = entity;
        }

        entt::entity rangeAnchor() const
        {
            return m_state.range_anchor;
        }

        void setRangeAnchor(entt::entity entity)
        {
            m_state.range_anchor = entity;
        }

    private:
        EditorSelectionModel m_state;
    };
} // namespace Hybrid
