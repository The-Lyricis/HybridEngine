#include "scene.h"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "runtime/core/base/math_util.h"
#include <runtime/modules/scene/components/rigidbody_component.h>

namespace Hybrid
{
    namespace
    {
        bool hasTransform(const entt::registry& reg, entt::entity e)
        {
            return e != entt::null && reg.valid(e) && reg.all_of<TransformComponent>(e);
        }

        glm::mat4 composeLocalMatrix(const TransformComponent& tc)
        {
            const glm::mat4 T = glm::translate(glm::mat4(1.0f), tc.Position);
            const glm::mat4 R = MathUtil::mat4FromQuat(tc.Rotation);
            const glm::mat4 S = glm::scale(glm::mat4(1.0f), tc.Scale);
            return T * R * S;
        }

        bool decomposeLocalMatrix(const glm::mat4& local, TransformComponent& tc)
        {
            glm::vec3 scale{};
            glm::quat rotation{};
            glm::vec3 translation{};
            glm::vec3 skew{};
            glm::vec4 perspective{};
            if (!glm::decompose(local, scale, rotation, translation, skew, perspective))
                return false;

            tc.Position = translation;
            tc.Scale = scale;
            tc.Rotation = MathUtil::normalizeQuat(rotation);
            return true;
        }

        void markDirtyRecursiveImpl(entt::registry& reg, entt::entity root)
        {
            if (!hasTransform(reg, root))
                return;

            auto& tc = reg.get<TransformComponent>(root);
            tc.DirtyWorld = true;

            entt::entity child = tc.FirstChild;
            while (child != entt::null)
            {
                entt::entity next = entt::null;
                if (hasTransform(reg, child))
                    next = reg.get<TransformComponent>(child).NextSibling;

                markDirtyRecursiveImpl(reg, child);
                child = next;
            }
        }

        void updateNodeRecursive(entt::registry& reg,
            entt::entity node,
            const glm::mat4& parent_world,
            bool parent_world_dirty)
        {
            if (!hasTransform(reg, node))
                return;

            auto& tc = reg.get<TransformComponent>(node);

            bool local_changed = false;
            if (tc.DirtyLocal)
            {
                tc.LocalMatrix = composeLocalMatrix(tc);
                tc.DirtyLocal = false;
                local_changed = true;
            }

            const bool world_changed = parent_world_dirty || tc.DirtyWorld || local_changed;
            if (world_changed)
            {
                tc.WorldMatrix = parent_world * tc.LocalMatrix;
                tc.DirtyWorld = false;
            }

            const glm::mat4 world_for_children = tc.WorldMatrix;

            entt::entity child = tc.FirstChild;
            while (child != entt::null)
            {
                entt::entity next = entt::null;
                if (hasTransform(reg, child))
                    next = reg.get<TransformComponent>(child).NextSibling;

                updateNodeRecursive(reg, child, world_for_children, world_changed);
                child = next;
            }
        }
    } // namespace

    // -------------------------
    // UUID / Entity Map
    // -------------------------

    void Scene::onEntityCreated(Entity e)
    {
        // 容错：保证 IDComponent 存在
        if (!e.HasComponent<IDComponent>())
            e.AddComponent<IDComponent>(IDComponent{ UUIDGenerator::New() });

        const auto id = e.GetComponent<IDComponent>().ID;
        if (id.value != 0)
            m_EntityMap[id.value] = e.GetHandle();
    }

    void Scene::onEntityDestroyed(entt::entity h)
    {
        if (!m_Registry.valid(h))
            return;

        if (auto* id = m_Registry.try_get<IDComponent>(h))
        {
            if (id->ID.value != 0)
                m_EntityMap.erase(id->ID.value);
        }
    }

    Entity Scene::findEntityByUUID(UUID id) const
    {
        if (id.value == 0)
            return Entity{};

        auto it = m_EntityMap.find(id.value);
        if (it == m_EntityMap.end())
            return Entity{};

        if (!m_Registry.valid(it->second))
            return Entity{};

        // 你的 Entity 包装如果只接受非 const 指针，这里需要 const_cast
        return Entity(it->second, const_cast<entt::registry*>(&m_Registry), const_cast<Scene*>(this));
    }

    // -------------------------
    // Create / Destroy
    // -------------------------

    Entity Scene::createEntity(const std::string& name)
    {
        return createEntityWithUUID(UUIDGenerator::New(), name);
    }

    Entity Scene::createEntityWithUUID(UUID id, const std::string& name)
    {
        entt::entity handle = m_Registry.create();
        Entity entity(handle, &m_Registry, this);

        entity.AddComponent<IDComponent>(IDComponent{ id });
        entity.AddComponent<TagComponent>(TagComponent{ name });
        entity.AddComponent<TransformComponent>();

        onEntityCreated(entity);
        return entity;
    }

    Entity Scene::createCameraEntity(const std::string& name, bool primary)
    {
        Entity e = createEntity(name);
        auto& cam = e.AddComponent<CameraComponent>();
        cam.Primary = primary;
        return e;
    }

    Entity Scene::createRenderableEntity(const std::string& name)
    {
        Entity e = createEntity(name);
        e.AddComponent<MeshRendererComponent>();
        return e;
    }

    void Scene::destroyEntity(Entity entity)
    {
        DestroyEntityRecursive(entity);
    }

    void Scene::DestroyEntityRecursive(Entity e)
    {
        if (!e.IsValid() || e.GetScene() != this)
            return;

        const entt::entity h = e.GetHandle();
        if (!m_Registry.valid(h))
            return;

        // 先递归销毁孩子
        if (hasTransform(m_Registry, h))
        {
            entt::entity child = m_Registry.get<TransformComponent>(h).FirstChild;
            while (child != entt::null)
            {
                entt::entity next = entt::null;
                if (hasTransform(m_Registry, child))
                    next = m_Registry.get<TransformComponent>(child).NextSibling;

                DestroyEntityRecursive(Entity(child, &m_Registry, this));
                child = next;
            }

            // 再统一断链（修复父节点 firstChild 悬空问题）
            detachFromParentLinks(h);
        }

        onEntityDestroyed(h);
        m_Registry.destroy(h);
    }

    // -------------------------
    // Parent Links (moved into Scene)
    // -------------------------

    void Scene::detachFromParentLinks(entt::entity child)
    {
        if (!hasTransform(m_Registry, child))
            return;

        auto& ct = m_Registry.get<TransformComponent>(child);

        // 修复父节点 FirstChild
        if (hasTransform(m_Registry, ct.Parent))
        {
            auto& pt = m_Registry.get<TransformComponent>(ct.Parent);
            if (pt.FirstChild == child)
                pt.FirstChild = ct.NextSibling;
        }

        // 修复兄弟链
        if (hasTransform(m_Registry, ct.PrevSibling))
            m_Registry.get<TransformComponent>(ct.PrevSibling).NextSibling = ct.NextSibling;

        if (hasTransform(m_Registry, ct.NextSibling))
            m_Registry.get<TransformComponent>(ct.NextSibling).PrevSibling = ct.PrevSibling;

        ct.Parent = entt::null;
        ct.NextSibling = entt::null;
        ct.PrevSibling = entt::null;
    }

    void Scene::attachToParentLinks(entt::entity child, entt::entity parent)
    {
        if (!hasTransform(m_Registry, child))
            return;

        auto& ct = m_Registry.get<TransformComponent>(child);
        ct.Parent = parent;
        ct.PrevSibling = entt::null;
        ct.NextSibling = entt::null;

        if (!hasTransform(m_Registry, parent))
            return;

        auto& pt = m_Registry.get<TransformComponent>(parent);
        ct.NextSibling = pt.FirstChild;
        if (hasTransform(m_Registry, pt.FirstChild))
            m_Registry.get<TransformComponent>(pt.FirstChild).PrevSibling = child;

        pt.FirstChild = child;
    }

    // -------------------------
    // Relationship ops
    // -------------------------

    bool Scene::SetParent(Entity child, Entity parent, bool worldPositionStays)
    {
        if (!child.IsValid() || child.GetScene() != this)
            return false;

        const entt::entity child_h = child.GetHandle();
        if (!hasTransform(m_Registry, child_h))
            return false;

        entt::entity parent_h = entt::null;
        if (parent.IsValid())
        {
            if (parent.GetScene() != this)
                return false;

            parent_h = parent.GetHandle();
            if (!hasTransform(m_Registry, parent_h))
                return false;

            if (parent_h == child_h)
                return false;

            // 注意：这里我们用了“严格后代”语义，避免 self=true 的困惑
            if (IsDescendant(parent, child))
                return false;
        }

        auto& child_tc = m_Registry.get<TransformComponent>(child_h);
        if (child_tc.Parent == parent_h)
            return true;

        glm::mat4 old_world{ 1.0f };
        if (worldPositionStays)
        {
            updateTransformHierarchy();
            old_world = child_tc.WorldMatrix;
        }

        detachFromParentLinks(child_h);
        attachToParentLinks(child_h, parent_h);

        if (worldPositionStays)
        {
            glm::mat4 parent_world{ 1.0f };
            if (hasTransform(m_Registry, parent_h))
                parent_world = m_Registry.get<TransformComponent>(parent_h).WorldMatrix;

            const glm::mat4 new_local = glm::inverse(parent_world) * old_world;
            auto& updated_tc = m_Registry.get<TransformComponent>(child_h);
            if (decomposeLocalMatrix(new_local, updated_tc))
            {
                updated_tc.LocalMatrix = new_local;
                updated_tc.DirtyLocal = false;
            }
            else
            {
                updated_tc.DirtyLocal = true;
            }
        }

        MarkDirtyRecursive(child);
        return true;
    }

    bool Scene::Detach(Entity child, bool worldPositionStays)
    {
        return SetParent(child, Entity{}, worldPositionStays);
    }

    bool Scene::IsDescendant(Entity node, Entity ancestor) const
    {
        if (!node.IsValid() || !ancestor.IsValid())
            return false;
        if (node.GetScene() != this || ancestor.GetScene() != this)
            return false;

        const entt::entity ancestor_h = ancestor.GetHandle();
        entt::entity current = node.GetHandle();

        // 严格后代：先跳到 parent，再判断
        while (hasTransform(m_Registry, current))
        {
            current = m_Registry.get<TransformComponent>(current).Parent;
            if (current == entt::null)
                break;
            if (current == ancestor_h)
                return true;
        }
        return false;
    }

    void Scene::MarkDirtyRecursive(Entity root)
    {
        if (!root.IsValid() || root.GetScene() != this)
            return;
        markDirtyRecursiveImpl(m_Registry, root.GetHandle());
    }

    // -------------------------
    // Update
    // -------------------------

    void Scene::onUpdate(float dt)
    {
        (void)dt;
        updateTransformHierarchy();
    }

    std::vector<Entity> Scene::getRootEntities() const
    {
        std::vector<Entity> roots;
        auto view = m_Registry.view<TransformComponent>();
        roots.reserve(view.size());

        for (auto e : view)
        {
            const auto& tc = view.get<TransformComponent>(e);
            if (tc.Parent == entt::null)
            {
                roots.emplace_back(e, const_cast<entt::registry*>(&m_Registry), const_cast<Scene*>(this));
            }
        }
        return roots;
    }

    std::shared_ptr<Scene> Scene::cloneRuntime() const
    {
        auto dst = std::make_shared<Scene>();
        dst->setName(m_Name);

        const auto& srcReg = m_Registry;
        auto& dstReg = dst->getRegistry();

        std::unordered_map<entt::entity, entt::entity> entityMap;

        auto srcView = srcReg.view<const IDComponent, const TagComponent, const TransformComponent>();
        for (auto srcEntity : srcView)
        {
            const auto& srcID = srcView.get<const IDComponent>(srcEntity);
            const auto& srcTag = srcView.get<const TagComponent>(srcEntity);

            Entity dstEntity = dst->createEntityWithUUID(srcID.ID, srcTag.Tag);
            entityMap[srcEntity] = dstEntity.GetHandle();
        }

        for (const auto& [srcEntity, dstEntity] : entityMap)
        {
            const auto& srcTc = srcReg.get<TransformComponent>(srcEntity);
            auto& dstTc = dstReg.get<TransformComponent>(dstEntity);

            dstTc.Position = srcTc.Position;
            dstTc.Rotation = srcTc.Rotation;
            dstTc.Scale = srcTc.Scale;
            dstTc.LocalMatrix = glm::mat4(1.0f);
            dstTc.WorldMatrix = glm::mat4(1.0f);
            dstTc.DirtyLocal = true;
            dstTc.DirtyWorld = true;
            dstTc.Parent = entt::null;
            dstTc.FirstChild = entt::null;
            dstTc.NextSibling = entt::null;
            dstTc.PrevSibling = entt::null;
        }

        for (const auto& [srcEntity, dstEntity] : entityMap)
        {
            if (srcReg.all_of<CameraComponent>(srcEntity))
                dstReg.emplace_or_replace<CameraComponent>(dstEntity, srcReg.get<CameraComponent>(srcEntity));

            if (srcReg.all_of<MeshRendererComponent>(srcEntity))
                dstReg.emplace_or_replace<MeshRendererComponent>(dstEntity, srcReg.get<MeshRendererComponent>(srcEntity));

            if (srcReg.all_of<DirectionalLightComponent>(srcEntity))
                dstReg.emplace_or_replace<DirectionalLightComponent>(dstEntity, srcReg.get<DirectionalLightComponent>(srcEntity));

            if (srcReg.all_of<PointLightComponent>(srcEntity))
                dstReg.emplace_or_replace<PointLightComponent>(dstEntity, srcReg.get<PointLightComponent>(srcEntity));

            if (srcReg.all_of<RigidbodyComponent>(srcEntity))
                dstReg.emplace_or_replace<RigidbodyComponent>(dstEntity, srcReg.get<RigidbodyComponent>(srcEntity));
        }

        auto remapEntity = [&entityMap](entt::entity e) -> entt::entity
        {
            if (e == entt::null)
                return entt::null;

            auto it = entityMap.find(e);
            return (it == entityMap.end()) ? entt::null : it->second;
        };

        for (const auto& [srcEntity, dstEntity] : entityMap)
        {
            const auto& srcTc = srcReg.get<TransformComponent>(srcEntity);
            auto& dstTc = dstReg.get<TransformComponent>(dstEntity);

            dstTc.Parent = remapEntity(srcTc.Parent);
            dstTc.FirstChild = remapEntity(srcTc.FirstChild);
            dstTc.NextSibling = remapEntity(srcTc.NextSibling);
            dstTc.PrevSibling = remapEntity(srcTc.PrevSibling);
            dstTc.DirtyWorld = true;
        }

        dst->onUpdate(0.0f);
        return dst;
    }

    void Scene::updateTransformHierarchy()
    {
        auto view = m_Registry.view<TransformComponent>();

        // 1) 修复非法 parent：用统一断链函数，避免悬空 FirstChild
        for (auto e : view)
        {
            auto& tc = view.get<TransformComponent>(e);
            if (tc.Parent != entt::null && !hasTransform(m_Registry, tc.Parent))
            {
                detachFromParentLinks(e);
                tc.DirtyWorld = true;
            }
        }

        // 2) 从根更新
        for (auto e : view)
        {
            const auto& tc = view.get<TransformComponent>(e);
            if (tc.Parent == entt::null)
                updateNodeRecursive(m_Registry, e, glm::mat4(1.0f), false);
        }
    }
    

} // namespace Hybrid
