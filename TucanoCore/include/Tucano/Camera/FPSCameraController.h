#pragma once

#include "Tucano/Camera/Camera.h"
#include <glm/glm.hpp>

namespace Tucano {

class FPSCameraController {
public:
    FPSCameraController(Camera* camera);

    void Update(float deltaTime);

    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const { return m_Position; }

    void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
    void SetLookSpeed(float speed) { m_LookSpeed = speed; }

private:
    void UpdateViewMatrix();

private:
    Camera* m_Camera;

    glm::vec3 m_Position{0.0f, 0.0f, 5.0f};
    glm::vec3 m_Forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_Up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_Right{1.0f, 0.0f, 0.0f};

    float m_Pitch = 0.0f;
    float m_Yaw = -90.0f; // Pointing to -Z by default

    float m_MoveSpeed = 5.0f;
    float m_LookSpeed = 0.1f;

    glm::vec2 m_LastMousePos{0.0f, 0.0f};
    bool m_FirstMouse = true;
    bool m_Active = false;
};

} // namespace Tucano
