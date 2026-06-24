#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Tucano {

struct Transform {
    glm::vec3 Position{0.0f};
    glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f}; // W, X, Y, Z
    glm::vec3 Scale{1.0f};

    Transform() = default;

    glm::mat4 GetModelMatrix() const {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), Position);
        glm::mat4 rotation = glm::toMat4(Rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), Scale);
        return translation * rotation * scale;
    }

    void Rotate(const glm::vec3& eulerAngles) {
        Rotation = glm::quat(eulerAngles) * Rotation;
        Rotation = glm::normalize(Rotation);
    }
};

} // namespace Tucano
