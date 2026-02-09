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
    // yaw: rotation around Y to point toward forward.xz (note coordinate convention)
    float yaw = glm::degrees(std::atan2(forward.x, forward.z));
    m_RotationDeg = glm::vec3(pitch, yaw, 0.0f);
    m_ViewDirty = false;
}

static glm::quat OrientationFromEulerDegrees(const glm::vec3& eulerDeg)
{
    // We want a deterministic convention: yaw (Y) then pitch (X) then roll (Z).
    // Build rotation matrix explicitly and convert to quaternion to avoid missing helper functions.
    float pitchRad = glm::radians(eulerDeg.x);
    float yawRad   = glm::radians(eulerDeg.y);
    float rollRad  = glm::radians(eulerDeg.z);

    glm::mat4 rot(1.0f);
    // Apply yaw (Y), then pitch (X), then roll (Z)
    rot = glm::rotate(rot, yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
    rot = glm::rotate(rot, pitchRad, glm::vec3(1.0f, 0.0f, 0.0f));
    rot = glm::rotate(rot, rollRad, glm::vec3(0.0f, 0.0f, 1.0f));

    return glm::quat_cast(rot);
}

glm::vec3 Camera::Forward() const
{
    glm::quat q = OrientationFromEulerDegrees(m_RotationDeg);
    // Camera looks down -Z in its local space
    return glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::Right() const
{
    glm::quat q = OrientationFromEulerDegrees(m_RotationDeg);
    return glm::normalize(q * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::Up() const
{
    glm::quat q = OrientationFromEulerDegrees(m_RotationDeg);
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
        // Build orientation deterministically from Euler angles and use its inverse for view.
        glm::quat orientation = OrientationFromEulerDegrees(m_RotationDeg);

        // view = inverse(rotation) * translate(-position)
        glm::mat4 rotMat = glm::toMat4(glm::conjugate(orientation)); // inverse of rotation
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), -m_Position);

        m_View = rotMat * trans;
        m_ViewDirty = false;
    }
}