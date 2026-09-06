#pragma once

#include <functional>
#include <memory>

#include "runtime/modules/render/runtime/extraction/visibility_collector.h"
#include "runtime/modules/render/runtime/frame_view_data.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/render_frame_request.h"

namespace Hybrid
{
    class AssetManager;
    class Mesh;
    class Scene;

    struct RenderPacketBuildInput
    {
        std::shared_ptr<Scene> scene;
        FrameViewData view;
        RenderEnvironmentData environment;
        const RenderShadowData* shadow = nullptr;
        const RenderViewRequest* view_request = nullptr;
        std::shared_ptr<AssetManager> asset_manager;
        MaterialSystem* material_system = nullptr;
        std::function<MeshGPU*(AssetID, const std::shared_ptr<Mesh>&)> resolve_mesh_gpu;
    };

    class RenderPacketBuilder
    {
    public:
        RenderPacket build(const RenderPacketBuildInput& input) const;

    private:
        void collectPacketLights(RenderPacket& packet) const;
        void collectPacketDrawItems(RenderPacket& packet,
                                    const VisibilityCollectInput& visibility_input,
                                    const Frustum& frustum) const;
        void collectShadowCasterItems(RenderPacket& packet,
                                      const VisibilityCollectInput& visibility_input) const;
        void sortRenderPacket(RenderPacket& packet) const;

    private:
        VisibilityCollector m_VisibilityCollector;
    };
} // namespace Hybrid
