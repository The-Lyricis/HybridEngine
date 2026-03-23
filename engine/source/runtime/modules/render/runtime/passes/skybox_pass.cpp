#include "skybox_pass.h"

#include <array>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_targets.h"

namespace Hybrid
{
    void SkyboxPass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.framebuffer;
        const std::shared_ptr<Shader>& skybox_shader = context.skybox_shader;

        if (!framebuffer || !skybox_shader || !packet.frame.useSkyboxClear || !packet.environment.skyboxTexture)
            return;

        SkyboxCubeGPU* cube = getOrCreateSkyboxCube();
        if (!cube)
            return;

        framebuffer->bind();
        framebuffer->setDrawColorAttachments({RenderTargets::kSceneColorAttachment});
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());

        RenderCommand::setBlendEnabled(false);
        RenderCommand::setDepthTestEnabled(true);
        RenderCommand::setDepthWriteEnabled(false);
        RenderCommand::setDepthCompareFunc(DepthCompareFunc::LessEqual);
        RenderCommand::setCullEnabled(false);

        skybox_shader->bind();
        skybox_shader->setFloat("u_Intensity", packet.environment.skyboxIntensity);
        skybox_shader->setFloat("u_RotationDegrees", packet.environment.skyboxRotationDegrees);
        packet.environment.skyboxTexture->bind(RenderBindings::kSkyboxCubemapSlot);

        cube->vao->bind();
        RenderCommand::drawIndexed(cube->index_count);

        RenderCommand::setDepthCompareFunc(DepthCompareFunc::Less);
        RenderCommand::setDepthWriteEnabled(true);
        RenderCommand::setCullEnabled(true);
        framebuffer->setDrawColorAttachments({
            RenderTargets::kSceneColorAttachment,
            RenderTargets::kSceneEntityIDAttachment
        });
        framebuffer->unbind();
    }

    SkyboxPass::SkyboxCubeGPU* SkyboxPass::getOrCreateSkyboxCube()
    {
        if (m_HasSkyboxCube)
            return &m_SkyboxCube;

        static constexpr std::array<float, 24> kVertices = {
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
        };

        static constexpr std::array<uint32_t, 36> kIndices = {
            0, 1, 2, 2, 3, 0,
            1, 5, 6, 6, 2, 1,
            5, 4, 7, 7, 6, 5,
            4, 0, 3, 3, 7, 4,
            3, 2, 6, 6, 7, 3,
            4, 5, 1, 1, 0, 4,
        };

        m_SkyboxCube.vb =
            VertexBuffer::Create(kVertices.data(), static_cast<uint32_t>(kVertices.size() * sizeof(float)));
        m_SkyboxCube.ib = IndexBuffer::Create(kIndices.data(), static_cast<uint32_t>(kIndices.size()));
        m_SkyboxCube.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(float) * 3;
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_SkyboxCube.vao->setVertexBuffer(m_SkyboxCube.vb, layout);
        m_SkyboxCube.vao->setIndexBuffer(m_SkyboxCube.ib);
        m_SkyboxCube.index_count = static_cast<uint32_t>(kIndices.size());
        m_HasSkyboxCube = true;
        return &m_SkyboxCube;
    }
} // namespace Hybrid
