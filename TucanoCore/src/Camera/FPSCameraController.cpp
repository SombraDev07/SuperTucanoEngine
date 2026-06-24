#include "Tucano/Camera/FPSCameraController.h"
#include "Tucano/Core/Input.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Tucano {

FPSCameraController::FPSCameraController(Camera* camera) : m_Camera(camera) {
    UpdateViewMatrix();
}

void FPSCameraController::Update(float deltaTime) {
    // Right click to enable camera look
    if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        if (!m_Active) {
            Input::SetCursorMode(GLFW_CURSOR_DISABLED);
            m_FirstMouse = true;
            m_Active = true;
        }
    } else {
        if (m_Active) {
            Input::SetCursorMode(GLFW_CURSOR_NORMAL);
            m_Active = false;
        }
    }

    if (m_Active) {
        glm::vec2 mousePos = Input::GetMousePosition();
        if (m_FirstMouse) {
            m_LastMousePos = mousePos;
            m_FirstMouse = false;
        }

        float xoffset = mousePos.x - m_LastMousePos.x;
        float yoffset = m_LastMousePos.y - mousePos.y; // reversed since y-coordinates go from bottom to top
        m_LastMousePos = mousePos;

        xoffset *= m_LookSpeed;
        yoffset *= m_LookSpeed;

        m_Yaw += xoffset;
        m_Pitch += yoffset;

        if (m_Pitch > 89.0f) m_Pitch = 89.0f;
        if (m_Pitch < -89.0f) m_Pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        front.y = sin(glm::radians(m_Pitch));
        front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        m_Forward = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_Up = glm::normalize(glm::cross(m_Right, m_Forward));

        float velocity = m_MoveSpeed * deltaTime;
        if (Input::IsKeyPressed(GLFW_KEY_W)) m_Position += m_Forward * velocity;
        if (Input::IsKeyPressed(GLFW_KEY_S)) m_Position -= m_Forward * velocity;
        if (Input::IsKeyPressed(GLFW_KEY_A)) m_Position -= m_Right * velocity;
        if (Input::IsKeyPressed(GLFW_KEY_D)) m_Position += m_Right * velocity;
        if (Input::IsKeyPressed(GLFW_KEY_E)) m_Position += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
        if (Input::IsKeyPressed(GLFW_KEY_Q)) m_Position -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
    }

    UpdateViewMatrix();
}

void FPSCameraController::SetPosition(const glm::vec3& position) {
    m_Position = position;
    UpdateViewMatrix();
}

void FPSCameraController::UpdateViewMatrix() {
    m_Camera->SetViewMatrix(glm::lookAt(m_Position, m_Position + m_Forward, m_Up));
}

} // namespace Tucano
