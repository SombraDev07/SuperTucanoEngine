#pragma once

#include <entt/entt.hpp>
#include <string>

namespace Tucano {

class World {
public:
    World() = default;

    // Create an empty entity with a TransformComponent and TagComponent
    entt::entity CreateEntity(const std::string& name = std::string());
    
    // Destroy an entity
    void DestroyEntity(entt::entity entity);

    // Get the underlying registry
    entt::registry& GetRegistry() { return m_Registry; }
    const entt::registry& GetRegistry() const { return m_Registry; }

private:
    entt::registry m_Registry;
};

} // namespace Tucano
