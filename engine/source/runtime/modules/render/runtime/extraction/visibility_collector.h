#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "runtime/core/base/intersection.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/render_packet.h"

namespace Hybrid
{
    class AssetManager;
    class Mesh;

    struct VisibilityCollectInput
    {
        RenderPacket* packet = nullptr;
        std::shared_ptr<AssetManager> asset_manager;
        MaterialSystem* material_system = nullptr;
        std::function<MeshGPU*(AssetID, const std::shared_ptr<Mesh>&)> resolve_mesh_gpu;
    };

    class VisibilityCollector
    {
    public:
        void collectFrustum(const VisibilityCollectInput& input,
                            const Frustum& frustum,
                            std::vector<RenderDrawItem>* opaque_items,
                            std::vector<RenderDrawItem>* transparent_items,
                            std::vector<RenderDrawItem>* shadow_items,
                            uint32_t* tested_items,
                            uint32_t* culled_items,
                            bool count_scene_totals) const;

        void collectVolume(const VisibilityCollectInput& input,
                           const ConvexVolume& volume,
                           std::vector<RenderDrawItem>* shadow_items) const;
    };
} // namespace Hybrid
