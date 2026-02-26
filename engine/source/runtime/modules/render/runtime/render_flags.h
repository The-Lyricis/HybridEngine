#pragma once

#include <cstdint>

namespace Hybrid
{
    // Bitmask that selects which render features/passes run this frame.
    enum class RenderFlags : uint32_t
    {
        None = 0,
        Forward = 1u << 0,           // Main color pass.
        PickingID = 1u << 1,         // Entity-ID output for picking.
        SelectionOutline = 1u << 2,  // Selection highlight/outline pass.
        Gizmos = 1u << 3,            // Editor gizmo overlay pass.
        Grid = 1u << 4,              // Editor/world grid overlay pass.
        Shadows = 1u << 5,           // Shadow map pass.
        PostProcess = 1u << 6,       // Post-process chain.
        DebugNormals = 1u << 7       // Debug normal visualization.
    };

    inline RenderFlags operator|(RenderFlags lhs, RenderFlags rhs)
    {
        return static_cast<RenderFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    inline RenderFlags operator&(RenderFlags lhs, RenderFlags rhs)
    {
        return static_cast<RenderFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    inline RenderFlags& operator|=(RenderFlags& lhs, RenderFlags rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    inline RenderFlags& operator&=(RenderFlags& lhs, RenderFlags rhs)
    {
        lhs = lhs & rhs;
        return lhs;
    }

    inline bool HasFlag(RenderFlags flags, RenderFlags bit)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0;
    }
} // namespace Hybrid
