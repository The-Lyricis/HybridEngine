#include "scene.h"

namespace Hybrid
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        entt::entity handle = m_Registry.create();

        Entity entity(handle, &m_Registry, this);

        // 默认组件：ID + Tag + Transform
        entity.AddComponent<IDComponent>(IDComponent{ m_NextID++ });
        entity.AddComponent<TagComponent>(TagComponent{ name });
        entity.AddComponent<TransformComponent>();

        return entity;
    }
    Entity Scene::CreateCameraEntity(const std::string& name, bool primary)
    {
        Entity e = CreateEntity(name);
        auto& cam = e.AddComponent<CameraComponent>();
        cam.Primary = primary;
        return e;
    }

    Entity Scene::CreateRenderableEntity(const std::string& name)
    {
        Entity e = CreateEntity(name);
        e.AddComponent<MeshRendererComponent>(); // 默认 primitive=0
        return e;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity.IsValid())
            return;

        m_Registry.destroy(entity.GetHandle());
    }

    void Scene::OnUpdate(float dt)
    {
        (void)dt;

        // 当前阶段先保持最小可运行：遍历 Transform
        // 后续你会在这里做：
        // 1) TransformSystem 更新
        // 2) RenderSystem 收集与提交
        auto view = m_Registry.view<TransformComponent>();
        for (auto e : view)
        {
            auto& tc = view.get<TransformComponent>(e);
            (void)tc;
        }
    }
}
