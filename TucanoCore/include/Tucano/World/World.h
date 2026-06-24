#pragma once

#include "Tucano/World/RenderableObject.h"
#include <vector>

namespace Tucano {

class World {
public:
    World() = default;

    void AddObject(const RenderableObject& object) {
        m_Objects.push_back(object);
    }

    const std::vector<RenderableObject>& GetObjects() const { return m_Objects; }
    std::vector<RenderableObject>& GetObjects() { return m_Objects; }

private:
    std::vector<RenderableObject> m_Objects;
};

} // namespace Tucano
