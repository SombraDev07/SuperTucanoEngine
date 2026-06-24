#include "Tucano/Camera/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Tucano {

Camera::Camera(float fov, float aspect, float nearClip, float farClip) {
    SetProjection(fov, aspect, nearClip, farClip);
}

void Camera::SetProjection(float fov, float aspect, float nearClip, float farClip) {
    m_FOV = fov;
    m_Aspect = aspect;
    m_Near = nearClip;
    m_Far = farClip;

    m_Projection = glm::perspective(glm::radians(fov), aspect, nearClip, farClip);
    
    // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted.
    // Vulkan expects the Y coordinate to point downwards. We fix it here.
    m_Projection[1][1] *= -1.0f;
}

void Camera::SetViewMatrix(const glm::mat4& view) {
    m_View = view;
}

} // namespace Tucano
