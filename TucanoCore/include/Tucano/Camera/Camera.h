#pragma once

#include <glm/glm.hpp>

namespace Tucano {

class Camera {
public:
    Camera(float fov, float aspect, float nearClip, float farClip);

    void SetProjection(float fov, float aspect, float nearClip, float farClip);
    void SetViewMatrix(const glm::mat4& view);

    const glm::mat4& GetProjection() const { return m_Projection; }
    const glm::mat4& GetView() const { return m_View; }

    float GetFOV() const { return m_FOV; }
    float GetAspect() const { return m_Aspect; }
    float GetNear() const { return m_Near; }
    float GetFar() const { return m_Far; }

private:
    glm::mat4 m_Projection{1.0f};
    glm::mat4 m_View{1.0f};

    float m_FOV;
    float m_Aspect;
    float m_Near;
    float m_Far;
};

} // namespace Tucano
