#include "scene.h"

#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "runtime/core/base/math_util.h"

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

        void detachFromParentLinks(entt::registry& reg, entt::entity child)
        {
            if (!hasTransform(reg, child))
                return;

            auto& ct = reg.get<TransformComponent>(child);

            if (hasTransform(reg, ct.Parent))
            {
                auto& pt = reg.get<TransformComponent>(ct.Parent);
                if (pt.FirstChild == child)
                {
                    pt.FirstChild = ct.NextSibling;
                }
            }

            if (hasTransform(reg, ct.PrevSibling))
            {
                reg.get<TransformComponent>(ct.PrevSibling).NextSibling = ct.NextSibling;
            }

            if (hasTransform(reg, ct.NextSibling))
            {
                reg.get<TransformComponent>(ct.NextSibling).PrevSibling = ct.PrevSibling;
            }

            ct.Parent = entt::null;
            ct.NextSibling = entt::null;
            ct.PrevSibling = entt::null;
        }

        void attachToParentLinks(entt::registry& reg, entt::entity child, entt::entity parent)
        {
            if (!hasTransform(reg, child))
                return;

            auto& ct = reg.get<TransformComponent>(child);
            ct.Parent = parent;
            ct.PrevSibling = entt::null;
            ct.NextSibling = entt::null;

            if (!hasTransform(reg, parent))
                return;

            auto& pt = reg.get<TransformComponent>(parent);
            ct.NextSibling = pt.FirstChild;
            if (hasTransform(reg, pt.FirstChild))
            {
                reg.get<TransformComponent>(pt.FirstChild).PrevSibling = child;
            }
            pt.FirstChild = child;
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
                {
                    next = reg.get<TransformComponent>(child).NextSibling;
                }

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
                {
                    next = reg.get<TransformComponent>(child).NextSibling;
                }

                updateNodeRecursive(reg, child, world_for_children, world_changed);
                child = next;
            }
        }
    } // namespace

    Entity Scene::createEntity(const std::string& name)
    {
        entt::entity handle = m_Registry.create();

        Entity entity(handle, &m_Registry, this);

        entity.AddComponent<IDComponent>(IDComponent{m_NextID++});
        entity.AddComponent<TagComponent>(TagComponent{name});
        entity.AddComponent<TransformComponent>();

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

            if (IsDescendant(parent, child))
                return false;
        }

        auto& child_tc = m_Registry.get<TransformComponent>(child_h);
        if (child_tc.Parent == parent_h)
            return true;

        glm::mat4 old_world{1.0f};
        if (worldPositionStays)
        {
            updateTransformHierarchy();
            old_world = child_tc.WorldMatrix;
        }

        detachFromParentLinks(m_Registry, child_h);
        attachToParentLinks(m_Registry, child_h, parent_h);

        if (worldPositionStays)
        {
            glm::mat4 parent_world{1.0f};
            if (hasTransform(m_Registry, parent_h))
            {
                parent_world = m_Registry.get<TransformComponent>(parent_h).WorldMatrix;
            }

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

        while (hasTransform(m_Registry, current))
        {
            if (current == ancestor_h)
                return true;

            current = m_Registry.get<TransformComponent>(current).Parent;
        }

        return false;
    }

    void Scene::MarkDirtyRecursive(Entity root)
    {
        if (!root.IsValid() || root.GetScene() != this)
            return;

        markDirtyRecursiveImpl(m_Registry, root.GetHandle());
    }

    void Scene::DestroyEntityRecursive(Entity e)
    {
        if (!e.IsValid() || e.GetScene() != this)
            return;

        const entt::entity h = e.GetHandle();
        if (!m_Registry.valid(h))
            return;

        if (hasTransform(m_Registry, h))
        {
            entt::entity child = m_Registry.get<TransformComponent>(h).FirstChild;
            while (child != entt::null)
            {
                entt::entity next = entt::null;
                if (hasTransform(m_Registry, child))
                {
                    next = m_Registry.get<TransformComponent>(child).NextSibling;
                }

                DestroyEntityRecursive(Entity(child, &m_Registry, this));
                child = next;
            }

            detachFromParentLinks(m_Registry, h);
        }

        m_Registry.destroy(h);
    }

    void Scene::destroyEntity(Entity entity)
    {
        DestroyEntityRecursive(entity);
    }

    void Scene::onUpdate(float dt)
    {
        (void)dt;
        updateTransformHierarchy();
    }

    void Scene::updateTransformHierarchy()
    {
        auto view = m_Registry.view<TransformComponent>();

        for (auto e : view)
        {
            auto& tc = view.get<TransformComponent>(e);
            if (tc.Parent != entt::null && !hasTransform(m_Registry, tc.Parent))
            {
                tc.Parent = entt::null;
                tc.NextSibling = entt::null;
                tc.PrevSibling = entt::null;
                tc.DirtyWorld = true;
            }
        }

        for (auto e : view)
        {
            const auto& tc = view.get<TransformComponent>(e);
            if (tc.Parent == entt::null)
            {
                updateNodeRecursive(m_Registry, e, glm::mat4(1.0f), false);
            }
        }
    }
} // namespace Hybrid
