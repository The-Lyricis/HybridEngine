#pragma once

#include <cstdint>

namespace Hybrid
{
    struct EditorImageHandle
    {
        uint32_t index = 0xffffffffu;
        uint32_t generation = 0;

        explicit operator bool() const { return index != 0xffffffffu && generation != 0; }
        bool operator==(const EditorImageHandle& other) const
        {
            return index == other.index && generation == other.generation;
        }
    };
} // namespace Hybrid
