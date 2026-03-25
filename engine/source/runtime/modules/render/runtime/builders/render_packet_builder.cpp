#include "render_packet_builder.h"

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>

#include "runtime/core/base/intersection.h"
#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/render/runtime/render_uniforms.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        bool sameDrawItemSubmesh(const RenderDrawItem& lhs, const RenderDrawItem& rhs)
        {
            return lhs.entityID == rhs.entityID && lhs.meshId.value == rhs.meshId.value &&
                   lhs.materialId.value == rhs.materialId.value && lhs.indexOffset == rhs.indexOffset &&
                   lhs.indexCount == rhs.indexCount;
        }

        glm::vec3 lightDirectionFromTransform(const TransformComponent& tr)
        {
            const glm::vec3 dir = glm::vec3(tr.WorldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            const float len = glm::length(dir);
            if (len < 1e-4f)
                return glm::vec3(0.0f, -1.0f, 0.0f);
            return dir / len;
        }

        uint32_t resolveActiveSelectionEntityID(const EditorRenderExt* editor_ext)
        {
            if (!editor_ext)
                return kInvalidEntityID;
            return editor_ext->selection.active_entity;
        }
    } // namespace

    RenderPacket RenderPacketBuilder::build(const RenderPacketBuildInput& input) const
    {
        RenderPacket packet;
        packet.scene = input.scene;
        packet.frame = input.view.frame;
        if (input.shadow)
            packet.shadow = *input.shadow;
        packet.showColliderDebug = input.editor_ext ? input.editor_ext->show_collider_debug : false;
        packet.showShadowDebug = input.editor_ext ? input.editor_ext->show_shadow_debug : false;
        packet.activeEntityID = resolveActiveSelectionEntityID(input.editor_ext);

        if (packet.scene)
        {
            const SceneEnvironmentSettings& environment = packet.scene->environment();
            packet.environment.skyboxCubemap = environment.skybox_cubemap;
            packet.environment.skyboxIntensity = environment.skybox_intensity;
            packet.environment.skyboxRotationDegrees = environment.skybox_rotation_degrees;
            if (environment.skybox_cubemap.value != 0 && input.resolve_cubemap)
                packet.environment.skyboxTexture = input.resolve_cubemap(environment.skybox_cubemap);
        }
        if (packet.frame.useSkyboxClear && !packet.environment.skyboxTexture && input.resolve_cubemap)
            packet.environment.skyboxTexture = input.resolve_cubemap({});

        if (!input.asset_manager || !input.material_system || !input.resolve_mesh_gpu)
            return packet;

        collectPacketLights(packet);
        VisibilityCollectInput visibility_input{};
        visibility_input.packet = &packet;
        visibility_input.asset_manager = input.asset_manager;
        visibility_input.material_system = input.material_system;
        visibility_input.resolve_mesh_gpu = input.resolve_mesh_gpu;
        const Frustum frustum = BuildFrustum(packet.frame.viewProj);
        collectPacketDrawItems(packet, visibility_input, frustum);
        collectShadowCasterItems(packet, visibility_input);
        sortRenderPacket(packet);
        return packet;
    }

    void RenderPacketBuilder::collectPacketLights(RenderPacket& packet) const
    {
        packet.lights.dir.intensity = 0.0f;
        packet.lights.points.clear();
        packet.lights.points.reserve(RenderUniforms::kMaxPointLights);

        if (!packet.scene)
            return;

        auto& registry = packet.scene->getRegistry();

        auto dir_view = registry.view<TransformComponent, DirectionalLightComponent>();
        for (auto entity : dir_view)
        {
            const auto& transform = dir_view.get<TransformComponent>(entity);
            const auto& light = dir_view.get<DirectionalLightComponent>(entity);
            if (!light.Enabled)
                continue;

            packet.lights.dir.color = light.Color;
            packet.lights.dir.intensity = light.Intensity;
            packet.lights.dir.direction = lightDirectionFromTransform(transform);
            break;
        }

        auto point_view = registry.view<TransformComponent, PointLightComponent>();
        for (auto entity : point_view)
        {
            if (static_cast<int>(packet.lights.points.size()) >= RenderUniforms::kMaxPointLights)
                break;

            const auto& transform = point_view.get<TransformComponent>(entity);
            const auto& light = point_view.get<PointLightComponent>(entity);
            if (!light.Enabled)
                continue;

            RenderPointLightData point{};
            point.color = light.Color;
            point.intensity = light.Intensity;
            point.position = glm::vec3(transform.WorldMatrix[3]);
            point.range = light.Range;
            packet.lights.points.push_back(point);
        }
    }


    void RenderPacketBuilder::collectPacketDrawItems(RenderPacket& packet,
                                                     const VisibilityCollectInput& visibility_input,
                                                     const Frustum& frustum) const
    {
        packet.opaque_items.clear();
        packet.transparent_items.clear();
        packet.tested_items = 0;
        packet.culled_items = 0;
        packet.scene_renderers = 0;
        packet.scene_submeshes = 0;

        if (packet.scene)
        {
            auto& registry = packet.scene->getRegistry();
            auto render_view = registry.view<TransformComponent, MeshRendererComponent>();
            packet.opaque_items.reserve(render_view.size_hint());
            packet.transparent_items.reserve(render_view.size_hint() / 4);
        }

        m_VisibilityCollector.collectFrustum(visibility_input,
                                             frustum,
                                             &packet.opaque_items,
                                             &packet.transparent_items,
                                             nullptr,
                                             &packet.tested_items,
                                             &packet.culled_items,
                                             true);
    }

    void RenderPacketBuilder::collectShadowCasterItems(RenderPacket& packet,
                                                       const VisibilityCollectInput& visibility_input) const
    {
        packet.shadow_caster_items.clear();
        if (!packet.shadow.enabled)
            return;

        std::vector<RenderDrawItem> gathered_items;
        for (uint32_t cascade_index = 0; cascade_index < packet.shadow.cascadeCount; ++cascade_index)
        {
            const auto& cascade = packet.shadow.cascades[cascade_index];
            if (!cascade.valid)
                continue;

            std::array<glm::vec3, 16> hull_points{};
            for (size_t i = 0; i < 8; ++i)
            {
                hull_points[i] = cascade.receiverCornersWS[i];
                hull_points[8 + i] = cascade.casterExtrudedCornersWS[i];
            }

            const ConvexVolume caster_volume = BuildConvexHullVolume(hull_points);
            if (caster_volume.Valid)
                m_VisibilityCollector.collectVolume(visibility_input, caster_volume, &gathered_items);
        }

        packet.shadow_caster_items = std::move(gathered_items);
        auto& items = packet.shadow_caster_items;
        std::sort(items.begin(), items.end(),
                  [](const RenderDrawItem& lhs, const RenderDrawItem& rhs)
                  {
                      if (lhs.materialId.value != rhs.materialId.value)
                          return lhs.materialId.value < rhs.materialId.value;
                      if (lhs.meshId.value != rhs.meshId.value)
                          return lhs.meshId.value < rhs.meshId.value;
                      if (lhs.entityID != rhs.entityID)
                          return lhs.entityID < rhs.entityID;
                      if (lhs.indexOffset != rhs.indexOffset)
                          return lhs.indexOffset < rhs.indexOffset;
                      return lhs.indexCount < rhs.indexCount;
                  });
        items.erase(std::unique(items.begin(), items.end(), sameDrawItemSubmesh), items.end());
    }

    void RenderPacketBuilder::sortRenderPacket(RenderPacket& packet) const
    {
        auto material_mesh_entity_sort = [](const RenderDrawItem& lhs, const RenderDrawItem& rhs)
        {
            if (lhs.materialId.value != rhs.materialId.value)
                return lhs.materialId.value < rhs.materialId.value;
            if (lhs.meshId.value != rhs.meshId.value)
                return lhs.meshId.value < rhs.meshId.value;
            return lhs.entityID < rhs.entityID;
        };

        std::sort(packet.opaque_items.begin(), packet.opaque_items.end(), material_mesh_entity_sort);
        std::sort(packet.shadow_caster_items.begin(), packet.shadow_caster_items.end(), material_mesh_entity_sort);

        const glm::vec3 camera_pos = packet.frame.cameraPos;
        std::sort(packet.transparent_items.begin(), packet.transparent_items.end(),
                  [camera_pos](const RenderDrawItem& lhs, const RenderDrawItem& rhs)
                  {
                      const glm::vec3 lhs_pos = glm::vec3(lhs.model[3]);
                      const glm::vec3 rhs_pos = glm::vec3(rhs.model[3]);
                      const glm::vec3 lhs_delta = lhs_pos - camera_pos;
                      const glm::vec3 rhs_delta = rhs_pos - camera_pos;
                      const float lhs_dist2 = glm::dot(lhs_delta, lhs_delta);
                      const float rhs_dist2 = glm::dot(rhs_delta, rhs_delta);
                      return lhs_dist2 > rhs_dist2;
                  });
    }
} // namespace Hybrid
