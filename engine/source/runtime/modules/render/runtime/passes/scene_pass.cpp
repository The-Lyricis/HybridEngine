#include "scene_pass.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/asset/material.h"

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

        uint32_t encodeEntityID(uint32_t entity_id)
        {
            return entity_id + 1u;
        }
    }

    void ScenePass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.framebuffer;
        const std::shared_ptr<Shader>& scene_shader = context.scene_shader;
        auto asset_manager = context.asset_manager;
        auto* material_system = context.material_system;

        if (!framebuffer)
            return;

        framebuffer->bind();
        setSceneFramebufferDrawBuffers(true);
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
        Renderer::beginFrame(packet.frame.clearColor);
        uint32_t zero = 0;
        glClearBufferuiv(GL_COLOR, 1, &zero);

        if (asset_manager && material_system && scene_shader && context.resolve_mesh_gpu)
        {
            scene_shader->bind();
            scene_shader->setMat4("u_ViewProjection", packet.frame.viewProj);
            scene_shader->setVec3("u_CameraPos", packet.frame.cameraPos);
            scene_shader->setVec3("u_DirLight.color", packet.lights.dir.color);
            scene_shader->setFloat("u_DirLight.intensity", packet.lights.dir.intensity);
            scene_shader->setVec3("u_DirLight.direction", packet.lights.dir.direction);
            scene_shader->setInt("u_PointCount", static_cast<int>(packet.lights.points.size()));

            for (int i = 0; i < static_cast<int>(packet.lights.points.size()) && i < 16; ++i)
            {
                const auto& p = packet.lights.points[i];
                std::string base = "u_PointLights[" + std::to_string(i) + "]";
                scene_shader->setVec3(base + ".color", p.color);
                scene_shader->setFloat(base + ".intensity", p.intensity);
                scene_shader->setVec3(base + ".position", p.position);
                scene_shader->setFloat(base + ".range", p.range);
            }

            scene_shader->setInt("u_AlbedoMap", 0);
            scene_shader->setInt("u_NormalMap", 1);
            scene_shader->setInt("u_MRMap", 2);
            scene_shader->setInt("u_AOMap", 3);
            scene_shader->setInt("u_EmissiveMap", 4);

            for (const auto& item : packet.items)
            {
                const AssetID mesh_id = item.meshId;
                if (mesh_id.value == 0)
                    continue;

                std::shared_ptr<Mesh> cpu_mesh = asset_manager->loadSync<Mesh>(mesh_id);
                if (!cpu_mesh)
                    continue;

                MeshGPU* mesh_gpu = context.resolve_mesh_gpu(mesh_id, cpu_mesh);
                if (!mesh_gpu)
                    continue;

                std::shared_ptr<Material> cpu_material;
                AssetID material_id = item.materialId;
                if (material_id.value != 0)
                    cpu_material = asset_manager->loadSync<Material>(material_id);
                if (!cpu_material && !mesh_gpu->submeshes.empty() && mesh_gpu->submeshes[0].material.value != 0)
                {
                    material_id = mesh_gpu->submeshes[0].material;
                    cpu_material = asset_manager->loadSync<Material>(material_id);
                }
                if (!cpu_material)
                {
                    cpu_material = asset_manager->getDefault<Material>();
                    material_id = AssetID{};
                }

                auto* material_gpu = material_system->getOrCreate(material_id, cpu_material);
                if (!material_gpu)
                    continue;

                for (const auto& submesh : mesh_gpu->submeshes)
                {
                    const MaterialSystem::MaterialGPU* use_material = material_gpu;
                    if (submesh.material.value != 0)
                    {
                        auto sub_material = asset_manager->loadSync<Material>(submesh.material);
                        if (sub_material)
                        {
                            if (auto* sub_material_gpu = material_system->getOrCreate(submesh.material, sub_material))
                                use_material = sub_material_gpu;
                        }
                    }

                    scene_shader->setMat4("u_Model", item.model);
                    scene_shader->setVec4("u_TintColor", item.tint);
                    scene_shader->setUInt("u_EntityID", encodeEntityID(item.entityID));
                    use_material->bind(*scene_shader);

                    mesh_gpu->vao->bind();
                    RenderCommand::drawIndexed(submesh.index_count, submesh.index_offset);
                }
            }
        }

        Renderer::endFrame();
        framebuffer->unbind();

        GLFWwindow* window = static_cast<GLFWwindow*>(context.window_handle);
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        RenderCommand::setViewport(0, 0, display_w, display_h);
        RenderCommand::setClearColor({0.08f, 0.08f, 0.09f, 1.0f});
        RenderCommand::clear();
    }
} // namespace Hybrid
