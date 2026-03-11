#pragma once

#include <functional>
#include <memory>

#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/mesh_gpu.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/shader_library.h"

namespace Hybrid
{
    class Framebuffer;
    class Shader;

    // Internal execution context consumed by render pipeline stages.
    struct RenderContext
    {
        const FrameContext* frame = nullptr;
        const RenderPacket* packet = nullptr;
        RenderFlags flags = RenderFlags::None;
        void* window_handle = nullptr;
        std::shared_ptr<Framebuffer> framebuffer;

        std::shared_ptr<AssetManager> asset_manager;
        ShaderLibrary* shader_library = nullptr;
        MaterialSystem* material_system = nullptr;
        std::function<MeshGPU*(AssetID, const std::shared_ptr<Mesh>&)> resolve_mesh_gpu;

        std::shared_ptr<Shader> mesh_shader;
        std::shared_ptr<Shader> box_colider_shader;
    };
} // namespace Hybrid
