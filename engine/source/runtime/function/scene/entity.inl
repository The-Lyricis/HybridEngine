#pragma once
#include <utility>
#include <cassert>

namespace Hybrid
{
    template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args)
    {
        assert(IsValid() && "Entity::AddComponent() called on invalid entity");
        assert(!HasComponent<T>() && "Entity already has this component");
        return m_Registry->emplace<T>(m_Handle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& Entity::GetComponent()
    {
        assert(IsValid() && "Entity::GetComponent() called on invalid entity");
        assert(HasComponent<T>() && "Entity does not have this component");
        return m_Registry->get<T>(m_Handle);
    }

    template<typename T>
    const T& Entity::GetComponent() const
    {
        assert(IsValid() && "Entity::GetComponent() const called on invalid entity");
        assert(HasComponent<T>() && "Entity does not have this component");
        return m_Registry->get<T>(m_Handle);
    }

    template<typename T>
    bool Entity::HasComponent() const
    {
        assert(IsValid() && "Entity::HasComponent() called on invalid entity");
        return m_Registry->any_of<T>(m_Handle);
    }

    template<typename T>
    void Entity::RemoveComponent()
    {
        assert(IsValid() && "Entity::RemoveComponent() called on invalid entity");
        assert(HasComponent<T>() && "Entity does not have this component");
        m_Registry->remove<T>(m_Handle);
    }

    inline bool Entity::IsValid() const
    {
        return (m_Registry != nullptr) && (m_Handle != entt::null) && m_Registry->valid(m_Handle);
    }
}
