#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

Camera::Camera(float fovDegrees, float aspect, float zNear, float zFar)
{
    SetPerspective(fovDegrees, aspect, zNear, zFar);
}

void Camera::SetPerspective(float fovDegrees, float aspect, float zNear, float zFar)
{
    m_FovDeg = fovDegrees;
    m_Aspect = aspect;
    m_Near = zNear;
    m_Far = zFar;
    m_ProjDirty = true;
}

void Camera::SetAspect(float aspect) { m_Aspect = aspect; m_ProjDirty = true; }
void Camera::SetNearFar(float zNear, float zFar) { m_Near = zNear; m_Far = zFar; m_ProjDirty = true; }

void Camera::SetPosition(const glm::vec3& pos) { m_Position = pos; m_ViewDirty = true; }
const glm::vec3& Camera::GetPosition() const noexcept { return m_Position; }

void Camera::SetRotation(const glm::vec3& eulerDegrees) { m_RotationDeg = eulerDegrees; m_ViewDirty = true; }
const glm::vec3& Camera::GetRotation() const noexcept { return m_RotationDeg; }

void Camera::Translate(const glm::vec3& delta) { m_Position += delta; m_ViewDirty = true; }
void Camera::Rotate(const glm::vec3& deltaEulerDegrees) { m_RotationDeg += deltaEulerDegrees; m_ViewDirty = true; }

void Camera::LookAt(const glm::vec3& target, const glm::vec3& up)
{
    // Compute view directly via glm::lookAt; also update rotation to match forward vector
    m_View = glm::lookAt(m_Position, target, up);
    // derive Euler from look direction (optional, keep consistent)
    glm::vec3 forward = glm::normalize(target - m_Position);
    float pitch = glm::degrees(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)));
    float yaw = glm::degrees(std::atan2(forward.x, forward.z)); // note: +X to the right, +Z forward
    m_RotationDeg = glm::vec3(pitch, yaw, 0.0f);
    m_ViewDirty = false;
}

glm::vec3 Camera::Forward() const
{
    // Build orientation quaternion from Euler (degrees -> radians)
    glm::vec3 rad = glm::radians(m_RotationDeg);
    glm::quat q = glm::quat(rad);
    // In this convention, camera looks down -Z in local space
    return glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::Right() const
{
    glm::vec3 rad = glm::radians(m_RotationDeg);
    glm::quat q = glm::quat(rad);
    return glm::normalize(q * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::Up() const
{
    glm::vec3 rad = glm::radians(m_RotationDeg);
    glm::quat q = glm::quat(rad);
    return glm::normalize(q * glm::vec3(0.0f, 1.0f, 0.0f));
}

const glm::mat4& Camera::GetViewMatrix()
{
    if (m_ViewDirty) RecomputeMatrices();
    return m_View;
}

const glm::mat4& Camera::GetProjectionMatrix()
{
    if (m_ProjDirty) RecomputeMatrices();
    return m_Proj;
}

glm::mat4 Camera::GetViewProjection()
{
    if (m_ViewDirty || m_ProjDirty) RecomputeMatrices();
    return m_Proj * m_View;
}

void Camera::MarkDirty() { m_ViewDirty = m_ProjDirty = true; }

void Camera::RecomputeMatrices() const
{
    if (m_ProjDirty) {
        // Projection with GLM uses radians
        m_Proj = glm::perspective(glm::radians(m_FovDeg), m_Aspect, m_Near, m_Far);
        // GLM's perspective produces clip-space with +Y up and -Z forward; if using Vulkan, flip Y
        // Caller can flip in shader or multiply by a correction if needed. Keep unflipped here.
        m_ProjDirty = false;
    }

    if (m_ViewDirty) {
        // Convert Euler (pitch, yaw, roll) in degrees to quaternion orientation
        glm::vec3 rad = glm::radians(m_RotationDeg);
        // Note: glm::quat expects (w,x,y,z) when constructed from euler via glm::quat(rad)
        glm::quat orientation = glm::quat(rad);
        // Build view matrix: translate then rotate (camera space)
        glm::mat4 rotMat = glm::toMat4(orientation);
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), -m_Position);
        // view = R^T * T  (since orientation rotates world opposite to camera)
        m_View = rotMat * trans;
        m_ViewDirty = false;
    }
}