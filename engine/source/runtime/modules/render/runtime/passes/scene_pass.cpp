#include "scene_pass.h"

#include <string>

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/runtime/pipeline/pipeline_state.h"
#include "runtime/modules/render/runtime/render_binding_layout.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_targets.h"

namespace Hybrid
{
    namespace
    {
        struct SceneDrawParameterBlock
        {
            glm::mat4 model{1.0f};
            glm::vec4 tint{1.0f};
            uint32_t entity_id = 0;
        };

        struct SceneShadowParameterBlock
        {
            int shadows_enabled = 0;
            int cascade_count = 0;
            std::array<glm::mat4, kMaxDirectionalShadowCascades> light_view_projections{};
            std::array<float, kMaxDirectionalShadowCascades> cascade_splits{};
            float strength = 1.0f;
            float bias_constant = 0.0005f;
            float bias_slope = 0.0015f;
        };

        uint32_t encodeEntityID(uint32_t entity_id)
        {
            return entity_id + 1u;
        }

        SceneDrawParameterBlock BuildSceneDrawParameterBlock(const RenderDrawItem& item)
        {
            SceneDrawParameterBlock block{};
            block.model = item.model;
            block.tint = item.tint;
            block.entity_id = encodeEntityID(item.entityID);
            return block;
        }

        SceneShadowParameterBlock BuildSceneShadowParameterBlock(const RenderShadowData& shadow,
                                                                 bool has_shadow_cascade,
                                                                 bool has_shadow_textures)
        {
            SceneShadowParameterBlock block{};
            block.shadows_enabled = (has_shadow_cascade && has_shadow_textures) ? 1 : 0;
            block.cascade_count = has_shadow_cascade ? static_cast<int>(shadow.cascadeCount) : 0;
            block.strength = shadow.strength;
            block.bias_constant = shadow.biasConstant;
            block.bias_slope = shadow.biasSlope;

            for (uint32_t cascade_index = 0; cascade_index < kMaxDirectionalShadowCascades; ++cascade_index)
            {
                const bool valid = cascade_index < shadow.cascadeCount && shadow.cascades[cascade_index].valid;
                block.light_view_projections[cascade_index] =
                    valid ? shadow.cascades[cascade_index].lightViewProjection : glm::mat4(1.0f);
                block.cascade_splits[cascade_index] = valid ? shadow.cascades[cascade_index].splitFar : 0.0f;
            }

            return block;
        }

        void ApplySceneDrawParameters(Shader& shader, const SceneDrawParameterBlock& block)
        {
            const RenderBindingLayoutDesc& layout = GetSceneDrawBindingLayout();
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneModelUniform))
                shader.setMat4(std::string(binding->name), block.model);
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneTintColorUniform))
                shader.setVec4(std::string(binding->name), block.tint);
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneEntityIDUniform))
                shader.setUInt(std::string(binding->name), block.entity_id);
        }

        void ApplySceneShadowParameters(Shader& shader, const SceneShadowParameterBlock& block)
        {
            const RenderBindingLayoutDesc& layout = GetSceneShadowBindingLayout();
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneShadowCascadeCountUniform))
                shader.setInt(std::string(binding->name), block.cascade_count);
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneShadowsEnabledUniform))
                shader.setInt(std::string(binding->name), block.shadows_enabled);
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneShadowStrengthUniform))
                shader.setFloat(std::string(binding->name), block.strength);
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneShadowBiasConstUniform))
                shader.setFloat(std::string(binding->name), block.bias_constant);
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneShadowBiasSlopeUniform))
                shader.setFloat(std::string(binding->name), block.bias_slope);

            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneLightViewProjectionsUniform))
            {
                const uint32_t array_count = std::min(binding->array_count, static_cast<uint32_t>(block.light_view_projections.size()));
                for (uint32_t cascade_index = 0; cascade_index < array_count; ++cascade_index)
                {
                    shader.setMat4(std::string(binding->name) + "[" + std::to_string(cascade_index) + "]",
                                   block.light_view_projections[cascade_index]);
                }
            }

            if (const RenderBindingDesc* binding = FindRenderBinding(layout, RenderBindings::kSceneShadowCascadeSplitsUniform))
            {
                const uint32_t array_count = std::min(binding->array_count, static_cast<uint32_t>(block.cascade_splits.size()));
                for (uint32_t cascade_index = 0; cascade_index < array_count; ++cascade_index)
                {
                    shader.setFloat(std::string(binding->name) + "[" + std::to_string(cascade_index) + "]",
                                    block.cascade_splits[cascade_index]);
                }
            }
        }
    }

    void ScenePass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.framebuffer;
        const std::shared_ptr<Shader>& scene_shader = context.scene_shader;

        if (!framebuffer)
            return;

        framebuffer->bind();
        framebuffer->setDrawColorAttachments({
            RenderTargets::kSceneColorAttachment,
            RenderTargets::kSceneEntityIDAttachment
        });
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
        Renderer::beginFrame(packet.frame.clearColor);
        framebuffer->clearColorAttachmentUInt(RenderTargets::kSceneEntityIDAttachment, 0);

        if (scene_shader)
        {
            scene_shader->bind();
            const bool has_shadow_cascade = packet.shadow.enabled && packet.shadow.cascadeCount > 0 &&
                                            packet.shadow.cascades[0].valid;
            ApplySceneShadowParameters(*scene_shader,
                                       BuildSceneShadowParameterBlock(packet.shadow,
                                                                     has_shadow_cascade,
                                                                     context.shadow_cascade_framebuffers != nullptr));
            if (has_shadow_cascade && context.shadow_cascade_framebuffers)
            {
                for (uint32_t cascade_index = 0; cascade_index < packet.shadow.cascadeCount; ++cascade_index)
                {
                    const std::shared_ptr<Framebuffer>& shadow_fb = (*context.shadow_cascade_framebuffers)[cascade_index];
                    if (shadow_fb)
                        shadow_fb->bindDepthAttachmentTexture(RenderBindings::kSceneShadowMapSlot + cascade_index);
                }
            }

            auto draw_queue = [&](const std::vector<RenderDrawItem>& items)
            {
                for (const auto& item : items)
                {
                    if (!item.meshGPU || !item.materialGPU || item.indexCount == 0)
                        continue;

                    ApplySceneDrawParameters(*scene_shader, BuildSceneDrawParameterBlock(item));
                    item.materialGPU->bind(*scene_shader);

                    item.meshGPU->vao->bind();
                    RenderCommand::drawIndexed(item.indexCount, item.indexOffset);
                }
            };

            ApplyPipelineState(PipelineStates::OpaqueDepth());
            draw_queue(packet.opaque_items);

            if (!packet.transparent_items.empty())
            {
                ScopedPipelineState transparent_state(PipelineStates::TransparentDepthRead());
                draw_queue(packet.transparent_items);
            }
        }

        Renderer::endFrame();
        framebuffer->unbind();
    }
} // namespace Hybrid
