#include "Tucano/World/World.h"
#include "Tucano/ECS/Components.h"

namespace Tucano {

entt::entity World::CreateEntity(const std::string& name) {
    entt::entity entity = m_Registry.create();
    m_Registry.emplace<TransformComponent>(entity);
    
    auto& tag = m_Registry.emplace<TagComponent>(entity);
    tag.Tag = name.empty() ? "Entity" : name;
    
    return entity;
}

void World::DestroyEntity(entt::entity entity) {
    m_Registry.destroy(entity);
}

} // namespace Tucano
