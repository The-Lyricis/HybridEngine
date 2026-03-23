#pragma once

#include <memory>

#include "runtime/modules/render/runtime/render_context.h"

namespace Hybrid
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    class GizmoPass
    {
    public:
        void execute(RenderContext& context);

    private:
        struct DebugLineMeshGPU
        {
            std::shared_ptr<VertexArray> vao;
            std::shared_ptr<VertexBuffer> vb;
            std::shared_ptr<IndexBuffer> ib;
            uint32_t index_count = 0;
        };

        void drawSelectedColliderGizmo(RenderContext& context,
                                       Shader& shader,
                                       DebugLineMeshGPU& debug_box_mesh) const;
        void drawSelectedCameraGizmo(RenderContext& context,
                                     Shader& shader,
                                     DebugLineMeshGPU& debug_box_mesh,
                                     DebugLineMeshGPU& debug_quad_mesh) const;
        void drawSelectedPointLightGizmo(RenderContext& context,
                                         Shader& shader,
                                         DebugLineMeshGPU& debug_sphere_mesh) const;
        void drawSelectedDirectionalLightGizmo(RenderContext& context,
                                               Shader& shader,
                                               DebugLineMeshGPU& directional_light_mesh) const;
        void drawShadowDebugGizmos(const RenderPacket& packet,
                                   Shader& shader,
                                   DebugLineMeshGPU& shadow_hull_mesh) const;

        DebugLineMeshGPU* getOrCreateDebugBoxMeshGPU();
        DebugLineMeshGPU* getOrCreateDebugSphereMeshGPU();
        DebugLineMeshGPU* getOrCreateDirectionalLightMeshGPU();
        DebugLineMeshGPU* getOrCreateShadowHullMeshGPU();
        DebugLineMeshGPU* getOrCreateDebugQuadMeshGPU();

    private:
        DebugLineMeshGPU m_DebugBoxMeshGPU;
        DebugLineMeshGPU m_DebugSphereMeshGPU;
        DebugLineMeshGPU m_DirectionalLightMeshGPU;
        DebugLineMeshGPU m_ShadowHullMeshGPU;
        DebugLineMeshGPU m_DebugQuadMeshGPU;
        bool m_HasDebugBoxMeshGPU = false;
        bool m_HasDebugSphereMeshGPU = false;
        bool m_HasDirectionalLightMeshGPU = false;
        bool m_HasShadowHullMeshGPU = false;
        bool m_HasDebugQuadMeshGPU = false;
    };
} // namespace Hybrid
