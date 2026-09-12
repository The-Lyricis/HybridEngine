#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace Hybrid
{
    template<typename Handle, typename Value>
    class HandlePool
    {
    public:
        Handle create(Value value)
        {
            for (uint32_t index = 0; index < m_slots.size(); ++index)
            {
                Slot& slot = m_slots[index];
                if (!slot.alive)
                {
                    slot.value = std::move(value);
                    slot.alive = true;
                    return {index, slot.generation};
                }
            }
            Slot slot;
            slot.value = std::move(value);
            slot.alive = true;
            m_slots.push_back(std::move(slot));
            return {static_cast<uint32_t>(m_slots.size() - 1), m_slots.back().generation};
        }

        Value* get(Handle handle)
        {
            if (!isValid(handle)) return nullptr;
            return &m_slots[handle.index].value;
        }

        const Value* get(Handle handle) const
        {
            if (!isValid(handle)) return nullptr;
            return &m_slots[handle.index].value;
        }

        bool destroy(Handle handle)
        {
            if (!isValid(handle)) return false;
            Slot& slot = m_slots[handle.index];
            slot.value = Value{};
            slot.alive = false;
            ++slot.generation;
            if (slot.generation == 0) slot.generation = 1;
            return true;
        }

        bool isValid(Handle handle) const
        {
            return handle.isValid() && handle.index < m_slots.size() &&
                   m_slots[handle.index].alive &&
                   m_slots[handle.index].generation == handle.generation;
        }

        template<typename Function>
        void forEachAlive(Function&& function)
        {
            for (Slot& slot : m_slots)
            {
                if (slot.alive)
                    function(slot.value);
            }
        }

    private:
        struct Slot
        {
            Value value{};
            uint32_t generation = 1;
            bool alive = false;
        };
        std::vector<Slot> m_slots;
    };
} // namespace Hybrid
