#pragma once

#include "Tucano/Math/Transform.h"
#include "Tucano/Mesh/Mesh.h"
#include <memory>

namespace Tucano {

struct RenderableObject {
    Transform Transform;
    std::shared_ptr<Mesh> Mesh;

    RenderableObject(std::shared_ptr<Tucano::Mesh> mesh) : Mesh(mesh) {}
};

} // namespace Tucano
