#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Clamp pitch to avoid gimbal-lock singularity at exactly ±90°.
static float ClampPitch(float degrees)
{
    return std::clamp(degrees, -89.0f, 89.0f);
}

// Build a normalised forward vector from pitch (X) and yaw (Y) in degrees.
// Convention: yaw 0 looks down -Z, +yaw turns right (toward +X).
static glm::vec3 ForwardFromPitchYaw(float pitchDeg, float yawDeg)
{
    float p = glm::radians(pitchDeg);
    float y = glm::radians(yawDeg);
    return glm::normalize(glm::vec3(
        std::cos(p) * std::sin(y),   // x
        std::sin(p),                  // y
        -std::cos(p) * std::cos(y)    // z  (negative: default forward is -Z)
    ));
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Camera::Camera(float fovDegrees, float aspect, float zNear, float zFar)
{
    SetPerspective(fovDegrees, aspect, zNear, zFar);
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Transform setters
// ---------------------------------------------------------------------------

void Camera::SetPosition(const glm::vec3& pos) { m_Position = pos; m_ViewDirty = true; }
const glm::vec3& Camera::GetPosition() const noexcept { return m_Position; }

void Camera::SetRotation(const glm::vec3& eulerDegrees)
{
    m_Pitch = ClampPitch(eulerDegrees.x);
    m_Yaw = eulerDegrees.y;
    m_Roll = eulerDegrees.z;
    m_ViewDirty = true;
}

const glm::vec3& Camera::GetRotation() const noexcept
{
    // Return a temporary — kept as a static thread_local so the reference stays valid.
    thread_local glm::vec3 rot;
    rot = glm::vec3(m_Pitch, m_Yaw, m_Roll);
    return rot;
}

void Camera::Translate(const glm::vec3& delta)
{
    m_Position += delta;
    m_ViewDirty = true;
}

void Camera::Rotate(const glm::vec3& deltaEulerDegrees)
{
    m_Pitch = ClampPitch(m_Pitch + deltaEulerDegrees.x);
    m_Yaw += deltaEulerDegrees.y;
    m_Roll += deltaEulerDegrees.z;
    m_ViewDirty = true;
}

// ---------------------------------------------------------------------------
// LookAt
// ---------------------------------------------------------------------------

void Camera::LookAt(const glm::vec3& target, const glm::vec3& /*up*/)
{
    glm::vec3 dir = target - m_Position;
    float len = glm::length(dir);
    if (len < 1e-6f)
        return; // target is on top of camera

    dir /= len; // normalize

    // Derive pitch and yaw that are consistent with ForwardFromPitchYaw().
    m_Pitch = ClampPitch(glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f))));
    m_Yaw = glm::degrees(std::atan2(dir.x, -dir.z)); // -z because default forward is -Z
    m_Roll = 0.0f;

    m_ViewDirty = true;
}

// ---------------------------------------------------------------------------
// Orientation queries
// ---------------------------------------------------------------------------

glm::vec3 Camera::Forward() const
{
    return ForwardFromPitchYaw(m_Pitch, m_Yaw);
}

glm::vec3 Camera::Right() const
{
    // Right = cross(forward, worldUp), then normalise
    glm::vec3 fwd = Forward();
    return glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::Up() const
{
    return glm::normalize(glm::cross(Right(), Forward()));
}

// ---------------------------------------------------------------------------
// Matrix accessors (lazy evaluation)
// ---------------------------------------------------------------------------

const glm::mat4& Camera::GetViewMatrix()
{
    if (m_ViewDirty) RecomputeView();
    return m_View;
}

const glm::mat4& Camera::GetProjectionMatrix()
{
    if (m_ProjDirty) RecomputeProj();
    return m_Proj;
}

glm::mat4 Camera::GetViewProjection()
{
    if (m_ViewDirty) RecomputeView();
    if (m_ProjDirty) RecomputeProj();
    return m_Proj * m_View;
}

void Camera::MarkDirty()
{
    m_ViewDirty = m_ProjDirty = true;
}

// ---------------------------------------------------------------------------
// Matrix recomputation
// ---------------------------------------------------------------------------

void Camera::RecomputeView() const
{
    // Build view with glm::lookAt — guaranteed consistent with Forward().
    glm::vec3 fwd = ForwardFromPitchYaw(m_Pitch, m_Yaw);
    glm::vec3 target = m_Position + fwd;
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    m_View = glm::lookAt(m_Position, target, up);
    m_ViewDirty = false;
}

void Camera::RecomputeProj() const
{
    m_Proj = glm::perspective(glm::radians(m_FovDeg), m_Aspect, m_Near, m_Far);

    // Vulkan clip-space has +Y pointing downward.  Flip the Y axis of the
    // projection so geometry isn't inverted or incorrectly culled.
    m_Proj[1][1] *= -1.0f;

    m_ProjDirty = false;
}