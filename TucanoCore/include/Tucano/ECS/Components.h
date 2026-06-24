#pragma once

#include "Tucano/Math/Transform.h"
#include "Tucano/Mesh/Mesh.h"
#include "Tucano/Render/Material.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Tucano {

    // Unique Identifier Component
    struct TagComponent {
        std::string Tag;

        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    // Transform is now natively an ECS component
    struct TransformComponent {
        Transform Transform;

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const Tucano::Transform& transform) : Transform(transform) {}
    };

    // Mesh is now an ECS component
    struct MeshComponent {
        std::shared_ptr<Mesh> Mesh;

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
        MeshComponent(std::shared_ptr<Tucano::Mesh> mesh) : Mesh(mesh) {}
    };

    // Material component
    struct MaterialComponent {
        std::shared_ptr<Material> Material;

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
        MaterialComponent(std::shared_ptr<Tucano::Material> material) : Material(material) {}
    };

    // Camera Component
    struct CameraComponent {
        bool Primary = true;
        // The actual projection/view is still handled by our Camera class logic for now, 
        // but we can flag an entity as possessing the camera.
    };

} // namespace Tucano
