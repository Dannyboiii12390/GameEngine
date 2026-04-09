#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/glm.hpp>

class Camera
{
public:
    enum class ProjectionType : uint8_t
    {
        Perspective = 0,
        Orthographic = 1
    };

    Camera() = default;
    Camera(float fovDegrees, float aspect, float zNear, float zFar);

    // Configure projection
    void SetPerspective(float fovDegrees, float aspect, float zNear, float zFar);
    void SetOrthographic(float size, float aspect, float zNear, float zFar);
    void SetAspect(float aspect);
    void SetNearFar(float zNear, float zFar);

    // Get projection parameters
    ProjectionType GetProjectionType() const noexcept { return m_ProjectionType; }
    float GetFovDeg() const noexcept { return m_FovDeg; }
    float GetAspect() const noexcept { return m_Aspect; }
    float GetNear() const noexcept { return m_Near; }
    float GetFar() const noexcept { return m_Far; }
    float GetOrthoSize() const noexcept { return m_OrthoSize; }

    // Transform
    void SetPosition(const glm::vec3& pos);
    const glm::vec3& GetPosition() const noexcept;

    // Euler angles in degrees: x = pitch, y = yaw, z = roll
    void SetRotation(const glm::vec3& eulerDegrees);
    const glm::vec3& GetRotation() const noexcept;

    // Incremental movement / rotation
    void Translate(const glm::vec3& delta);
    void Rotate(const glm::vec3& deltaEulerDegrees);

    // Look-at helper (rebuilds internal Euler state to match)
    void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

    // Query orientation vectors (normalized, world-space)
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;

    // Get matrices (lazy-evaluated)
    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    glm::mat4 GetViewProjection();

    // Force matrix recompute on next query
    void MarkDirty();

private:
    void RecomputeView() const;
    void RecomputeProj() const;

    // Transform
    glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
    float m_Pitch{ 0.0f }; // degrees, rotation around local X (look up/down)
    float m_Yaw{ 0.0f };   // degrees, rotation around world Y (look left/right)
    float m_Roll{ 0.0f };  // degrees, rotation around local Z (tilt)

    // Projection params
    ProjectionType m_ProjectionType{ ProjectionType::Perspective };
    float m_FovDeg{ 60.0f };
    float m_OrthoSize{ 10.0f }; // height of orthographic view volume
    float m_Aspect{ 16.0f / 9.0f };
    float m_Near{ 0.1f };
    float m_Far{ 1000.0f };

    // Cached matrices
    mutable glm::mat4 m_View{ 1.0f };
    mutable glm::mat4 m_Proj{ 1.0f };
    mutable bool m_ViewDirty{ true };
    mutable bool m_ProjDirty{ true };
};