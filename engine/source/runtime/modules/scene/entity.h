#pragma once
#include <entt/entt.hpp>
#include <cstdint>

namespace Hybrid
{
    class Scene;

    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, entt::registry* registry, Scene* scene = nullptr)
            : m_Handle(handle), m_Registry(registry), m_Scene(scene) {
        }

        // --- Component API ---
        template<typename T, typename... Args>
        T& AddComponent(Args&&... args);

        template<typename T>
        T& GetComponent();

        template<typename T>
        const T& GetComponent() const;

        template<typename T>
        bool HasComponent() const;

        template<typename T>
        void RemoveComponent();

        // --- Basic ---
        bool IsValid() const;
        entt::entity GetHandle() const { return m_Handle; }
        Scene* GetScene() const { return m_Scene; }

        explicit operator bool() const { return IsValid(); }

        // 方便作为 key 使用
        uint32_t ToUInt() const { return static_cast<uint32_t>(m_Handle); }

    private:
        entt::entity m_Handle{ entt::null };
        entt::registry* m_Registry{ nullptr };
        Scene* m_Scene{ nullptr };
    };
}

#include "entity.inl"
