#pragma once

#include <cstdint>

#include "runtime/modules/render/rhi/rhi_types.h"

namespace Hybrid
{
    class IRenderDevice;

    // Backend-local bridge for adapters that must hand an OpenGL texture to a
    // legacy OpenGL consumer (Framebuffer/ImGui). Native IDs never cross the
    // public RHI or editor panel boundary.
    RhiResult<uint64_t> ResolveOpenGLTextureNativeHandle(IRenderDevice& device,
                                                         TextureViewHandle view);
} // namespace Hybrid
