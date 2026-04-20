#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct Waypoint
{
    glm::vec3 position;
    glm::vec3 rotation; // Euler angles in degrees
    float time;         // Absolute time in seconds
};

enum class EasingType : int8_t
{
    LINEAR = 0,
    SMOOTHSTEP = 1
};

enum class PathMode : int8_t
{
    STOP = 0,
    LOOP = 1,
    REVERSE = 2
};

class ComponentAnimation
{
public:
    ComponentAnimation();

    // Waypoint management
    void AddWaypoint(const glm::vec3& position, const glm::vec3& rotation, float absoluteTime);
    void SetWaypoints(const std::vector<Waypoint>& waypoints);
    const std::vector<Waypoint>& GetWaypoints() const { return m_waypoints; }

    // Animation settings
    void SetEasingType(EasingType easingType) { m_easingType = easingType; }
    void SetPathMode(PathMode pathMode) { m_pathMode = pathMode; }
    void SetTotalDuration(float duration) { m_totalDuration = duration; }

    EasingType GetEasingType() const { return m_easingType; }
    PathMode GetPathMode() const { return m_pathMode; }
    float GetTotalDuration() const { return m_totalDuration; }
    bool IsPlaying() const { return m_isPlaying; }

    // Animation control
    void Play() { m_isPlaying = true; m_currentTime = 0.0f; }
    void Pause() { m_isPlaying = false; }
    void Stop() { m_isPlaying = false; m_currentTime = 0.0f; }
    void SetTime(float time) { m_currentTime = time; }

    // Update and interpolation
    void Update(float deltaTime);
    void GetInterpolatedTransform(glm::vec3& outPosition, glm::vec3& outRotation, float& outAlpha) const;

    float GetCurrentTime() const { return m_currentTime; }

private:
    // Helper methods for interpolation
    void FindCurrentSegment(float normalizedTime, size_t& outSegmentStart, size_t& outSegmentEnd, float& outLocalT) const;
    glm::vec3 InterpolatePosition(const glm::vec3& p0, const glm::vec3& p1, float t) const;
    glm::vec3 InterpolateRotation(const glm::vec3& r0, const glm::vec3& r1, float t) const;
    float ApplyEasing(float t) const;

    std::vector<Waypoint> m_waypoints;
    EasingType m_easingType = EasingType::LINEAR;
    PathMode m_pathMode = PathMode::STOP;
    float m_totalDuration = 0.0f;
    float m_currentTime = 0.0f;
    bool m_isPlaying = true;
    bool m_isReversing = false; // For REVERSE path mode
};