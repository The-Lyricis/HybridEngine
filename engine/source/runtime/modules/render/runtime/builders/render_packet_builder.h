#pragma once

#include <functional>
#include <memory>

#include "runtime/modules/render/runtime/builders/shadow_frame_builder.h"
#include "runtime/modules/render/runtime/extraction/visibility_collector.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/render_packet.h"

namespace Hybrid
{
    class AssetManager;
    class Mesh;
    class Scene;

    struct RenderPacketBuildInput
    {
        std::shared_ptr<Scene> scene;
        FrameViewData view;
        const RenderShadowData* shadow = nullptr;
        const EditorRenderExt* editor_ext = nullptr;
        std::shared_ptr<AssetManager> asset_manager;
        MaterialSystem* material_system = nullptr;
        std::function<TexturePtr(AssetID)> resolve_cubemap;
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
