#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif


#include <glm/glm.hpp>

class Camera
{
public:
    Camera() = default;
    Camera(float fovDegrees, float aspect, float zNear, float zFar);

    // Configure projection
    void SetPerspective(float fovDegrees, float aspect, float zNear, float zFar);
    void SetAspect(float aspect);
    void SetNearFar(float zNear, float zFar);

    // Transform
    void SetPosition(const glm::vec3& pos);
    const glm::vec3& GetPosition() const noexcept;

    // Rotation expressed as Euler angles in degrees: (pitch, yaw, roll)
    void SetRotation(const glm::vec3& eulerDegrees);
    const glm::vec3& GetRotation() const noexcept;

    // Incremental movement / rotation
    void Translate(const glm::vec3& delta);
    void Rotate(const glm::vec3& deltaEulerDegrees);

    // Look at helper
    void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

    // Query orientation vectors (normalized)
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;

    // Get matrices
    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    glm::mat4 GetViewProjection(); // computed on request

    // Convenience
    void MarkDirty(); // force matrix recompute on next query

private:
    void RecomputeMatrices() const;

    // transform
    glm::vec3 m_Position{ 0.0f };
    glm::vec3 m_RotationDeg{ 0.0f }; // Euler angles (pitch, yaw, roll) in degrees

    // projection params
    float m_FovDeg{ 60.0f };
    float m_Aspect{ 16.0f / 9.0f };
    float m_Near{ 0.1f };
    float m_Far{ 1000.0f };

    // cached matrices
    mutable glm::mat4 m_View{ 1.0f };
    mutable glm::mat4 m_Proj{ 1.0f };
    mutable bool m_ViewDirty{ true };
    mutable bool m_ProjDirty{ true };
};