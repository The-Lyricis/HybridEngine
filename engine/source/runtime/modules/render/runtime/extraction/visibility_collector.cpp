#include "visibility_collector.h"

#include <memory>

#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        struct ResolvedRenderable
        {
            const TransformComponent* transform = nullptr;
            const MeshRendererComponent* renderer = nullptr;
            MeshGPU* mesh_gpu = nullptr;
            AssetID base_material_id{};
            const MaterialSystem::MaterialGPU* base_material_gpu = nullptr;
        };

        bool resolveRenderable(const VisibilityCollectInput& input,
                               entt::entity entity,
                               ResolvedRenderable& out_renderable)
        {
            if (!input.packet || !input.packet->scene || !input.asset_manager || !input.material_system ||
                !input.resolve_mesh_gpu)
                return false;

            auto& registry = input.packet->scene->getRegistry();
            if (!registry.valid(entity) || !registry.all_of<TransformComponent, MeshRendererComponent>(entity))
                return false;

            const auto& transform = registry.get<TransformComponent>(entity);
            const auto& renderer = registry.get<MeshRendererComponent>(entity);
            if (!renderer.Enabled || renderer.Mesh.value == 0)
                return false;

            std::shared_ptr<Mesh> cpu_mesh = input.asset_manager->loadSync<Mesh>(renderer.Mesh);
            if (!cpu_mesh)
                return false;

            MeshGPU* mesh_gpu = input.resolve_mesh_gpu(renderer.Mesh, cpu_mesh);
            if (!mesh_gpu)
                return false;

            std::shared_ptr<Material> base_material;
            AssetID base_material_id = renderer.Material;
            if (base_material_id.value != 0)
                base_material = input.asset_manager->loadSync<Material>(base_material_id);

            if (!base_material && !mesh_gpu->submeshes.empty() && mesh_gpu->submeshes[0].material.value != 0)
            {
                base_material_id = mesh_gpu->submeshes[0].material;
                base_material = input.asset_manager->loadSync<Material>(base_material_id);
            }

            if (!base_material)
            {
                base_material = input.asset_manager->getDefault<Material>();
                base_material_id = AssetID{};
            }

            auto* base_material_gpu = input.material_system->getOrCreate(base_material_id, base_material);
            if (!base_material_gpu)
                return false;

            out_renderable.transform = &transform;
            out_renderable.renderer = &renderer;
            out_renderable.mesh_gpu = mesh_gpu;
            out_renderable.base_material_id = base_material_id;
            out_renderable.base_material_gpu = base_material_gpu;
            return true;
        }
    } // namespace

    void VisibilityCollector::collectFrustum(const VisibilityCollectInput& input,
                                             const Frustum& frustum,
                                             std::vector<RenderDrawItem>* opaque_items,
                                             std::vector<RenderDrawItem>* transparent_items,
                                             std::vector<RenderDrawItem>* shadow_items,
                                             uint32_t* tested_items,
                                             uint32_t* culled_items,
                                             bool count_scene_totals) const
    {
        if (!input.packet || !input.packet->scene)
            return;

        auto& registry = input.packet->scene->getRegistry();
        auto render_view = registry.view<TransformComponent, MeshRendererComponent>();

        for (auto entity : render_view)
        {
            ResolvedRenderable resolved{};
            if (!resolveRenderable(input, entity, resolved))
                continue;

            if (count_scene_totals)
                ++input.packet->scene_renderers;

            for (const auto& submesh : resolved.mesh_gpu->submeshes)
            {
                if (count_scene_totals)
                    ++input.packet->scene_submeshes;
                if (tested_items)
                    ++(*tested_items);

                const AABB local_bounds{submesh.aabb_min, submesh.aabb_max};
                const AABB world_bounds = TransformAABB(local_bounds, resolved.transform->WorldMatrix);
                if (!IntersectsFrustum(frustum, world_bounds))
                {
                    if (culled_items)
                        ++(*culled_items);
                    continue;
                }

                AssetID effective_material_id = resolved.base_material_id;
                const MaterialSystem::MaterialGPU* effective_material_gpu = resolved.base_material_gpu;

                if (submesh.material.value != 0)
                {
                    auto sub_material = input.asset_manager->loadSync<Material>(submesh.material);
                    if (sub_material)
                    {
                        if (auto* sub_material_gpu = input.material_system->getOrCreate(submesh.material, sub_material))
                        {
                            effective_material_id = submesh.material;
                            effective_material_gpu = sub_material_gpu;
                        }
                    }
                }

                RenderDrawItem item{};
                item.meshId = resolved.renderer->Mesh;
                item.materialId = effective_material_id;
                item.meshGPU = resolved.mesh_gpu;
                item.materialGPU = effective_material_gpu;
                item.indexOffset = submesh.index_offset;
                item.indexCount = submesh.index_count;
                item.model = resolved.transform->WorldMatrix;
                item.tint = resolved.renderer->Tint;
                item.entityID = static_cast<uint32_t>(entt::to_integral(entity));

                const MaterialAlphaMode alpha_mode = effective_material_gpu->alphaMode();

                if (shadow_items)
                {
                    if (effective_material_gpu->castsShadow())
                        shadow_items->push_back(item);
                    continue;
                }

                if (alpha_mode == MaterialAlphaMode::Blend)
                {
                    if (transparent_items)
                        transparent_items->push_back(item);
                }
                else
                {
                    if (opaque_items)
                        opaque_items->push_back(item);
                }
            }
        }
    }

    void VisibilityCollector::collectVolume(const VisibilityCollectInput& input,
                                            const ConvexVolume& volume,
                                            std::vector<RenderDrawItem>* shadow_items) const
    {
        if (!input.packet || !input.packet->scene || !shadow_items || !volume.Valid)
            return;

        auto& registry = input.packet->scene->getRegistry();
        auto render_view = registry.view<TransformComponent, MeshRendererComponent>();

        for (auto entity : render_view)
        {
            ResolvedRenderable resolved{};
            if (!resolveRenderable(input, entity, resolved))
                continue;

            for (const auto& submesh : resolved.mesh_gpu->submeshes)
            {
                const AABB local_bounds{submesh.aabb_min, submesh.aabb_max};
                const AABB world_bounds = TransformAABB(local_bounds, resolved.transform->WorldMatrix);
                if (!IntersectsConvexVolume(volume, world_bounds))
                    continue;

                AssetID effective_material_id = resolved.base_material_id;
                const MaterialSystem::MaterialGPU* effective_material_gpu = resolved.base_material_gpu;

                if (submesh.material.value != 0)
                {
                    auto sub_material = input.asset_manager->loadSync<Material>(submesh.material);
                    if (sub_material)
                    {
                        if (auto* sub_material_gpu = input.material_system->getOrCreate(submesh.material, sub_material))
                        {
                            effective_material_id = submesh.material;
                            effective_material_gpu = sub_material_gpu;
                        }
                    }
                }

                if (!effective_material_gpu->castsShadow())
                    continue;

                RenderDrawItem item{};
                item.meshId = resolved.renderer->Mesh;
                item.materialId = effective_material_id;
                item.meshGPU = resolved.mesh_gpu;
                item.materialGPU = effective_material_gpu;
                item.indexOffset = submesh.index_offset;
                item.indexCount = submesh.index_count;
                item.model = resolved.transform->WorldMatrix;
                item.tint = resolved.renderer->Tint;
                item.entityID = static_cast<uint32_t>(entt::to_integral(entity));
                shadow_items->push_back(item);
            }
        }
    }
} // namespace Hybrid
