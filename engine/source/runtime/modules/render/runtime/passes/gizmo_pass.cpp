#include "gizmo_pass.h"

#include <array>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include <glm/gtc/matrix_transform.hpp>

#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/components/collider_component.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        void setSceneFramebufferDrawBuffers(bool write_entity_id)
        {
            if (write_entity_id)
            {
                constexpr GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2, buffers);
            }
            else
            {
                constexpr GLenum buffer = GL_COLOR_ATTACHMENT0;
                glDrawBuffers(1, &buffer);
            }
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
        if (!debug_box_mesh)
            return;

        auto& registry = packet.scene->getRegistry();
        auto view = registry.view<TransformComponent, ColliderComponent>();

        framebuffer->bind();
        setSceneFramebufferDrawBuffers(false);
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glLineWidth(2.0f);

        collider_debug_shader->bind();
        collider_debug_shader->setMat4("u_ViewProjection", packet.frame.viewProj);

        for (auto e : view)
        {
            if (packet.activeEntityID == kInvalidEntityID || entt::to_integral(e) != packet.activeEntityID)
                continue;

            const auto& tr = view.get<TransformComponent>(e);
            const auto& col = view.get<ColliderComponent>(e);
            if (!col.Enabled || col.Type != ColliderType::Box)
                continue;

            glm::vec4 color = glm::vec4(0.2f, 0.95f, 0.35f, 1.0f);
            if (col.IsTrigger)
                color = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);

            glm::mat4 collider_local =
                glm::translate(glm::mat4(1.0f), col.Center) *
                glm::scale(glm::mat4(1.0f), col.Box.HalfExtents * 2.0f);

            const glm::mat4 model = tr.WorldMatrix * collider_local;
            collider_debug_shader->setMat4("u_Model", model);
            collider_debug_shader->setVec4("u_Color", color);

            debug_box_mesh->vao->bind();
            RenderCommand::drawLinesIndexed(debug_box_mesh->index_count);
        }

        glEnable(GL_CULL_FACE);
        setSceneFramebufferDrawBuffers(true);
        framebuffer->unbind();
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

        static constexpr std::array<uint32_t, 24> kBoxLineIndices = {
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7,
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
} // namespace Hybrid
