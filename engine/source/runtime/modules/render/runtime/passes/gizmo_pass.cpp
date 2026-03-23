#include "gizmo_pass.h"

#include <array>
#include <vector>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/render_targets.h"
#include "runtime/modules/render/runtime/render_uniforms.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/components/collider_component.h"
#include "runtime/modules/scene/components/directional_light_component.h"
#include "runtime/modules/scene/components/point_light_component.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        static constexpr std::array<uint32_t, 24> kBoxLineIndices = {
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7,
        };

        static constexpr std::array<uint32_t, 64> kShadowHullLineIndices = {
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7,
            8, 9, 9,10,10,11,11, 8,
           12,13,13,14,14,15,15,12,
            8,12, 9,13,10,14,11,15,
            0, 8, 1, 9, 2,10, 3,11,
            4,12, 5,13, 6,14, 7,15,
        };

        static constexpr std::array<uint32_t, 8> kQuadLineIndices = {
            0, 1, 1, 2, 2, 3, 3, 0,
        };

        std::array<glm::vec3, 8> buildCameraFrustumCorners(const TransformComponent& transform,
                                                           const CameraComponent& camera,
                                                           float aspect)
        {
            const float near_distance = std::max(camera.Near, 0.001f);
            const float far_distance = std::max(camera.Far, near_distance + 0.01f);
            const float tan_half_fov = std::tan(glm::radians(camera.FovY) * 0.5f);

            const auto write_plane = [&](float distance, size_t base_index, std::array<glm::vec3, 8>& corners)
            {
                const float half_height = distance * tan_half_fov;
                const float half_width = half_height * aspect;

                const std::array<glm::vec3, 4> local = {
                    glm::vec3(-half_width, -half_height, -distance),
                    glm::vec3( half_width, -half_height, -distance),
                    glm::vec3( half_width,  half_height, -distance),
                    glm::vec3(-half_width,  half_height, -distance),
                };

                for (size_t i = 0; i < local.size(); ++i)
                    corners[base_index + i] = glm::vec3(transform.WorldMatrix * glm::vec4(local[i], 1.0f));
            };

            std::array<glm::vec3, 8> corners{};
            write_plane(near_distance, 0, corners);
            write_plane(far_distance, 4, corners);
            return corners;
        }
    }

    void GizmoPass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.framebuffer;
        const std::shared_ptr<Shader>& collider_debug_shader = context.collider_debug_shader;

        if (!packet.showColliderDebug)
            return;

        if (!framebuffer || !packet.scene || !collider_debug_shader)
            return;

        auto* debug_box_mesh = getOrCreateDebugBoxMeshGPU();
        auto* debug_sphere_mesh = getOrCreateDebugSphereMeshGPU();
        auto* directional_light_mesh = getOrCreateDirectionalLightMeshGPU();
        auto* shadow_hull_mesh = getOrCreateShadowHullMeshGPU();
        auto* debug_quad_mesh = getOrCreateDebugQuadMeshGPU();
        if (!debug_box_mesh || !debug_sphere_mesh || !directional_light_mesh || !shadow_hull_mesh || !debug_quad_mesh)
            return;

        auto& registry = packet.scene->getRegistry();
        auto view = registry.view<TransformComponent, ColliderComponent>();
        auto camera_view = registry.view<TransformComponent, CameraComponent>();
        const float aspect = (packet.frame.gameAspect > 0.0f) ? packet.frame.gameAspect : (16.0f / 9.0f);

        framebuffer->bind();
        framebuffer->setDrawColorAttachments({RenderTargets::kSceneColorAttachment});
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());

        RenderCommand::setDepthTestEnabled(true);
        RenderCommand::setCullEnabled(false);
        RenderCommand::setLineWidth(2.0f);

        collider_debug_shader->bind();
        collider_debug_shader->setUniformBlockBinding(RenderUniforms::kFrameBlockName,
                                                      RenderUniforms::kFrameUBOBinding);
        drawSelectedColliderGizmo(context, *collider_debug_shader, *debug_box_mesh);
        drawShadowDebugGizmos(packet, *collider_debug_shader, *shadow_hull_mesh);
        drawSelectedCameraGizmo(context, *collider_debug_shader, *debug_box_mesh, *debug_quad_mesh);
        drawSelectedDirectionalLightGizmo(context, *collider_debug_shader, *directional_light_mesh);
        drawSelectedPointLightGizmo(context, *collider_debug_shader, *debug_sphere_mesh);

        RenderCommand::setCullEnabled(true);
        framebuffer->setDrawColorAttachments({
            RenderTargets::kSceneColorAttachment,
            RenderTargets::kSceneEntityIDAttachment
        });
        framebuffer->unbind();
    }

    void GizmoPass::drawSelectedColliderGizmo(RenderContext& context,
                                              Shader& shader,
                                              DebugLineMeshGPU& debug_box_mesh) const
    {
        const RenderPacket& packet = *context.packet;
        if (!packet.scene || packet.activeEntityID == kInvalidEntityID)
            return;

        auto& registry = packet.scene->getRegistry();
        const entt::entity selected_entity = static_cast<entt::entity>(packet.activeEntityID);
        if (!registry.valid(selected_entity) || !registry.all_of<TransformComponent, ColliderComponent>(selected_entity))
            return;

        const auto& tr = registry.get<TransformComponent>(selected_entity);
        const auto& col = registry.get<ColliderComponent>(selected_entity);
        if (!col.Enabled || col.Type != ColliderType::Box)
            return;

        glm::vec4 color = glm::vec4(0.2f, 0.95f, 0.35f, 1.0f);
        if (col.IsTrigger)
            color = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);

        const glm::mat4 collider_local =
            glm::translate(glm::mat4(1.0f), col.Center) *
            glm::scale(glm::mat4(1.0f), col.Box.HalfExtents * 2.0f);

        shader.setMat4("u_Model", tr.WorldMatrix * collider_local);
        shader.setVec4("u_Color", color);
        debug_box_mesh.vao->bind();
        RenderCommand::drawLinesIndexed(debug_box_mesh.index_count);
    }

    void GizmoPass::drawSelectedCameraGizmo(RenderContext& context,
                                            Shader& shader,
                                            DebugLineMeshGPU& debug_box_mesh,
                                            DebugLineMeshGPU& debug_quad_mesh) const
    {
        if (!context.packet || !context.packet->scene || !context.editor_selection ||
            context.editor_selection->active_entity == kInvalidEntityID)
            return;

        auto& registry = context.packet->scene->getRegistry();
        auto camera_view = registry.view<TransformComponent, CameraComponent>();
        const entt::entity selected_entity = static_cast<entt::entity>(context.editor_selection->active_entity);
        if (!registry.valid(selected_entity) || !camera_view.contains(selected_entity))
            return;

        const auto& camera_transform = camera_view.get<TransformComponent>(selected_entity);
        const auto& camera = camera_view.get<CameraComponent>(selected_entity);
        if (!camera.Enabled)
            return;

        const float aspect = (context.packet->frame.gameAspect > 0.0f)
                                 ? context.packet->frame.gameAspect
                                 : (16.0f / 9.0f);
        const std::array<glm::vec3, 8> frustum_vertices = buildCameraFrustumCorners(camera_transform, camera, aspect);

        debug_box_mesh.vb->setData(frustum_vertices.data(),
                                   static_cast<uint32_t>(frustum_vertices.size() * sizeof(glm::vec3)));
        shader.setMat4("u_Model", glm::mat4(1.0f));
        shader.setVec4("u_Color", glm::vec4(1.0f, 0.75f, 0.15f, 1.0f));
        debug_box_mesh.vao->bind();
        RenderCommand::drawLinesIndexed(debug_box_mesh.index_count);

        const std::array<glm::vec3, 4> near_plane_vertices{
            frustum_vertices[0],
            frustum_vertices[1],
            frustum_vertices[2],
            frustum_vertices[3],
        };
        debug_quad_mesh.vb->setData(near_plane_vertices.data(),
                                    static_cast<uint32_t>(near_plane_vertices.size() * sizeof(glm::vec3)));
        RenderCommand::setLineWidth(3.0f);
        shader.setVec4("u_Color", glm::vec4(1.0f, 0.95f, 0.35f, 1.0f));
        debug_quad_mesh.vao->bind();
        RenderCommand::drawLinesIndexed(debug_quad_mesh.index_count);
        RenderCommand::setLineWidth(1.5f);
    }

    void GizmoPass::drawSelectedPointLightGizmo(RenderContext& context,
                                                Shader& shader,
                                                DebugLineMeshGPU& debug_sphere_mesh) const
    {
        if (!context.packet || !context.packet->scene || !context.editor_selection ||
            context.editor_selection->active_entity == kInvalidEntityID)
            return;

        auto& registry = context.packet->scene->getRegistry();
        const entt::entity selected_entity = static_cast<entt::entity>(context.editor_selection->active_entity);
        if (!registry.valid(selected_entity) ||
            !registry.all_of<TransformComponent, PointLightComponent>(selected_entity))
            return;

        const auto& light_transform = registry.get<TransformComponent>(selected_entity);
        const auto& point_light = registry.get<PointLightComponent>(selected_entity);
        if (!point_light.Enabled || point_light.Range <= 0.0f)
            return;

        const glm::mat4 model =
            light_transform.WorldMatrix *
            glm::scale(glm::mat4(1.0f), glm::vec3(point_light.Range));
        shader.setMat4("u_Model", model);
        shader.setVec4("u_Color", glm::vec4(point_light.Color, 1.0f));
        debug_sphere_mesh.vao->bind();
        RenderCommand::drawLinesIndexed(debug_sphere_mesh.index_count);
    }

    void GizmoPass::drawSelectedDirectionalLightGizmo(RenderContext& context,
                                                      Shader& shader,
                                                      DebugLineMeshGPU& directional_light_mesh) const
    {
        if (!context.packet || !context.packet->scene || !context.editor_selection ||
            context.editor_selection->active_entity == kInvalidEntityID)
            return;

        auto& registry = context.packet->scene->getRegistry();
        const entt::entity selected_entity = static_cast<entt::entity>(context.editor_selection->active_entity);
        if (!registry.valid(selected_entity) ||
            !registry.all_of<TransformComponent, DirectionalLightComponent>(selected_entity))
            return;

        const auto& light_transform = registry.get<TransformComponent>(selected_entity);
        const auto& directional_light = registry.get<DirectionalLightComponent>(selected_entity);
        if (!directional_light.Enabled)
            return;

        shader.setMat4("u_Model", light_transform.WorldMatrix);
        shader.setVec4("u_Color", glm::vec4(directional_light.Color, 1.0f));
        directional_light_mesh.vao->bind();
        RenderCommand::drawLinesIndexed(directional_light_mesh.index_count);
    }

    void GizmoPass::drawShadowDebugGizmos(const RenderPacket& packet,
                                          Shader& shader,
                                          DebugLineMeshGPU& shadow_hull_mesh) const
    {
        if (!packet.showShadowDebug || !packet.shadow.enabled)
            return;

        RenderCommand::setLineWidth(1.5f);
        shader.setMat4("u_Model", glm::mat4(1.0f));

        for (uint32_t cascade_index = 0; cascade_index < packet.shadow.cascadeCount; ++cascade_index)
        {
            const auto& cascade = packet.shadow.cascades[cascade_index];
            if (!cascade.valid)
                continue;

            std::array<glm::vec3, 16> line_vertices{};
            for (size_t i = 0; i < 8; ++i)
            {
                line_vertices[i] = cascade.receiverCornersWS[i];
                line_vertices[8 + i] = cascade.casterExtrudedCornersWS[i];
            }

            shadow_hull_mesh.vb->setData(line_vertices.data(),
                                         static_cast<uint32_t>(line_vertices.size() * sizeof(glm::vec3)));

            const float t = (packet.shadow.cascadeCount > 1)
                                ? static_cast<float>(cascade_index) / static_cast<float>(packet.shadow.cascadeCount - 1)
                                : 0.0f;
            const glm::vec4 color = glm::mix(glm::vec4(0.10f, 0.90f, 1.0f, 1.0f),
                                             glm::vec4(1.0f, 0.45f, 0.10f, 1.0f),
                                             t);
            shader.setVec4("u_Color", color);
            shadow_hull_mesh.vao->bind();
            RenderCommand::drawLinesIndexed(shadow_hull_mesh.index_count);
        }
    }

    GizmoPass::DebugLineMeshGPU* GizmoPass::getOrCreateDebugBoxMeshGPU()
    {
        if (m_HasDebugBoxMeshGPU)
            return &m_DebugBoxMeshGPU;

        static constexpr std::array<glm::vec3, 8> kBoxVertices = {
            glm::vec3{-0.5f, -0.5f, -0.5f},
            glm::vec3{ 0.5f, -0.5f, -0.5f},
            glm::vec3{ 0.5f,  0.5f, -0.5f},
            glm::vec3{-0.5f,  0.5f, -0.5f},
            glm::vec3{-0.5f, -0.5f,  0.5f},
            glm::vec3{ 0.5f, -0.5f,  0.5f},
            glm::vec3{ 0.5f,  0.5f,  0.5f},
            glm::vec3{-0.5f,  0.5f,  0.5f},
        };

        m_DebugBoxMeshGPU.vb =
            VertexBuffer::Create(kBoxVertices.data(), static_cast<uint32_t>(kBoxVertices.size() * sizeof(glm::vec3)));
        m_DebugBoxMeshGPU.ib =
            IndexBuffer::Create(kBoxLineIndices.data(), static_cast<uint32_t>(kBoxLineIndices.size()));
        m_DebugBoxMeshGPU.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(glm::vec3);
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_DebugBoxMeshGPU.vao->setVertexBuffer(m_DebugBoxMeshGPU.vb, layout);
        m_DebugBoxMeshGPU.vao->setIndexBuffer(m_DebugBoxMeshGPU.ib);
        m_DebugBoxMeshGPU.index_count = static_cast<uint32_t>(kBoxLineIndices.size());
        m_HasDebugBoxMeshGPU = true;
        return &m_DebugBoxMeshGPU;
    }

    GizmoPass::DebugLineMeshGPU* GizmoPass::getOrCreateDebugSphereMeshGPU()
    {
        if (m_HasDebugSphereMeshGPU)
            return &m_DebugSphereMeshGPU;

        constexpr uint32_t kSegments = 48;
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(kSegments * 3);
        indices.reserve(kSegments * 3 * 2);

        const auto append_circle = [&](auto make_vertex)
        {
            const uint32_t base_index = static_cast<uint32_t>(vertices.size());
            for (uint32_t i = 0; i < kSegments; ++i)
            {
                const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(kSegments);
                vertices.push_back(make_vertex(angle));
            }

            for (uint32_t i = 0; i < kSegments; ++i)
            {
                indices.push_back(base_index + i);
                indices.push_back(base_index + ((i + 1) % kSegments));
            }
        };

        append_circle([](float angle)
        {
            return glm::vec3(std::cos(angle), std::sin(angle), 0.0f);
        });
        append_circle([](float angle)
        {
            return glm::vec3(std::cos(angle), 0.0f, std::sin(angle));
        });
        append_circle([](float angle)
        {
            return glm::vec3(0.0f, std::cos(angle), std::sin(angle));
        });

        m_DebugSphereMeshGPU.vb =
            VertexBuffer::Create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3)));
        m_DebugSphereMeshGPU.ib =
            IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        m_DebugSphereMeshGPU.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(glm::vec3);
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_DebugSphereMeshGPU.vao->setVertexBuffer(m_DebugSphereMeshGPU.vb, layout);
        m_DebugSphereMeshGPU.vao->setIndexBuffer(m_DebugSphereMeshGPU.ib);
        m_DebugSphereMeshGPU.index_count = static_cast<uint32_t>(indices.size());
        m_HasDebugSphereMeshGPU = true;
        return &m_DebugSphereMeshGPU;
    }

    GizmoPass::DebugLineMeshGPU* GizmoPass::getOrCreateDirectionalLightMeshGPU()
    {
        if (m_HasDirectionalLightMeshGPU)
            return &m_DirectionalLightMeshGPU;

        constexpr float kPlaneHalfExtent = 0.75f;
        constexpr float kArrowStartZ = -0.35f;
        constexpr float kArrowEndZ = -1.85f;
        constexpr float kArrowHeadLength = 0.22f;
        constexpr float kArrowHeadWidth = 0.12f;
        constexpr std::array<float, 3> kArrowOffsets = {-0.45f, 0.0f, 0.45f};

        std::vector<glm::vec3> vertices = {
            {-kPlaneHalfExtent, -kPlaneHalfExtent, 0.0f},
            { kPlaneHalfExtent, -kPlaneHalfExtent, 0.0f},
            { kPlaneHalfExtent,  kPlaneHalfExtent, 0.0f},
            {-kPlaneHalfExtent,  kPlaneHalfExtent, 0.0f},
        };
        std::vector<uint32_t> indices = {
            0, 1, 1, 2, 2, 3, 3, 0,
        };

        for (const float x_offset : kArrowOffsets)
        {
            const uint32_t base_index = static_cast<uint32_t>(vertices.size());
            vertices.push_back(glm::vec3(x_offset, 0.0f, kArrowStartZ));
            vertices.push_back(glm::vec3(x_offset, 0.0f, kArrowEndZ));
            vertices.push_back(glm::vec3(x_offset - kArrowHeadWidth, 0.0f, kArrowEndZ + kArrowHeadLength));
            vertices.push_back(glm::vec3(x_offset + kArrowHeadWidth, 0.0f, kArrowEndZ + kArrowHeadLength));
            vertices.push_back(glm::vec3(x_offset, -kArrowHeadWidth, kArrowEndZ + kArrowHeadLength));
            vertices.push_back(glm::vec3(x_offset,  kArrowHeadWidth, kArrowEndZ + kArrowHeadLength));

            indices.push_back(base_index + 0);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 2);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 3);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 4);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 5);
        }

        m_DirectionalLightMeshGPU.vb =
            VertexBuffer::Create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3)));
        m_DirectionalLightMeshGPU.ib =
            IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        m_DirectionalLightMeshGPU.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(glm::vec3);
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_DirectionalLightMeshGPU.vao->setVertexBuffer(m_DirectionalLightMeshGPU.vb, layout);
        m_DirectionalLightMeshGPU.vao->setIndexBuffer(m_DirectionalLightMeshGPU.ib);
        m_DirectionalLightMeshGPU.index_count = static_cast<uint32_t>(indices.size());
        m_HasDirectionalLightMeshGPU = true;
        return &m_DirectionalLightMeshGPU;
    }

    GizmoPass::DebugLineMeshGPU* GizmoPass::getOrCreateShadowHullMeshGPU()
    {
        if (m_HasShadowHullMeshGPU)
            return &m_ShadowHullMeshGPU;

        std::array<glm::vec3, 16> initial_vertices{};
        m_ShadowHullMeshGPU.vb =
            VertexBuffer::Create(initial_vertices.data(), static_cast<uint32_t>(initial_vertices.size() * sizeof(glm::vec3)));
        m_ShadowHullMeshGPU.ib =
            IndexBuffer::Create(kShadowHullLineIndices.data(), static_cast<uint32_t>(kShadowHullLineIndices.size()));
        m_ShadowHullMeshGPU.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(glm::vec3);
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_ShadowHullMeshGPU.vao->setVertexBuffer(m_ShadowHullMeshGPU.vb, layout);
        m_ShadowHullMeshGPU.vao->setIndexBuffer(m_ShadowHullMeshGPU.ib);
        m_ShadowHullMeshGPU.index_count = static_cast<uint32_t>(kShadowHullLineIndices.size());
        m_HasShadowHullMeshGPU = true;
        return &m_ShadowHullMeshGPU;
    }

    GizmoPass::DebugLineMeshGPU* GizmoPass::getOrCreateDebugQuadMeshGPU()
    {
        if (m_HasDebugQuadMeshGPU)
            return &m_DebugQuadMeshGPU;

        std::array<glm::vec3, 4> initial_vertices{};
        m_DebugQuadMeshGPU.vb =
            VertexBuffer::Create(initial_vertices.data(), static_cast<uint32_t>(initial_vertices.size() * sizeof(glm::vec3)));
        m_DebugQuadMeshGPU.ib =
            IndexBuffer::Create(kQuadLineIndices.data(), static_cast<uint32_t>(kQuadLineIndices.size()));
        m_DebugQuadMeshGPU.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(glm::vec3);
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_DebugQuadMeshGPU.vao->setVertexBuffer(m_DebugQuadMeshGPU.vb, layout);
        m_DebugQuadMeshGPU.vao->setIndexBuffer(m_DebugQuadMeshGPU.ib);
        m_DebugQuadMeshGPU.index_count = static_cast<uint32_t>(kQuadLineIndices.size());
        m_HasDebugQuadMeshGPU = true;
        return &m_DebugQuadMeshGPU;
    }
} // namespace Hybrid
