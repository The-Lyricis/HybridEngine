#pragma once

#include <cstdint>
#include <functional>

namespace Hybrid
{
    inline constexpr uint32_t kInvalidRhiHandleIndex = 0xffffffffu;

    template<typename Tag>
    struct RhiHandle
    {
        uint32_t index = kInvalidRhiHandleIndex;
        uint32_t generation = 0;

        bool isValid() const { return index != kInvalidRhiHandleIndex && generation != 0; }
        explicit operator bool() const { return isValid(); }
        bool operator==(const RhiHandle& rhs) const
        {
            return index == rhs.index && generation == rhs.generation;
        }
        bool operator!=(const RhiHandle& rhs) const { return !(*this == rhs); }
    };

    struct BufferHandleTag {};
    struct TextureHandleTag {};
    struct TextureViewHandleTag {};
    struct SamplerHandleTag {};
    struct ShaderHandleTag {};
    struct PipelineHandleTag {};

    using BufferHandle = RhiHandle<BufferHandleTag>;
    using TextureHandle = RhiHandle<TextureHandleTag>;
    using TextureViewHandle = RhiHandle<TextureViewHandleTag>;
    using SamplerHandle = RhiHandle<SamplerHandleTag>;
    using ShaderHandle = RhiHandle<ShaderHandleTag>;
    using PipelineHandle = RhiHandle<PipelineHandleTag>;
} // namespace Hybrid

namespace std
{
    template<typename Tag>
    struct hash<Hybrid::RhiHandle<Tag>>
    {
        size_t operator()(const Hybrid::RhiHandle<Tag>& handle) const noexcept
        {
            return (static_cast<size_t>(handle.generation) << 32u) ^ handle.index;
        }
    };
}
