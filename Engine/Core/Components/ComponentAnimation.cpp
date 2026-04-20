#define GLM_ENABLE_EXPERIMENTAL

#include "ComponentAnimation.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

ComponentAnimation::ComponentAnimation()
    : IComponent(EComponentType::Component_Animation),
      m_easingType(EasingType::LINEAR),
      m_pathMode(PathMode::STOP),
      m_totalDuration(0.0f),
      m_currentTime(0.0f),
      m_isPlaying(true),
      m_isReversing(false)
{
}

void ComponentAnimation::AddWaypoint(const glm::vec3& position, const glm::vec3& rotation, float absoluteTime)
{
    Waypoint wp;
    wp.position = position;
    wp.rotation = rotation;
    wp.time = absoluteTime;
    m_waypoints.push_back(wp);

    // Update total duration if this waypoint extends it
    if (absoluteTime > m_totalDuration)
    {
        m_totalDuration = absoluteTime;
    }

    std::cout << "Added waypoint at time " << absoluteTime << " pos: (" 
              << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

void ComponentAnimation::SetWaypoints(const std::vector<Waypoint>& waypoints)
{
    m_waypoints = waypoints;

    // Recalculate total duration
    m_totalDuration = 0.0f;
    for (const auto& wp : m_waypoints)
    {
        if (wp.time > m_totalDuration)
        {
            m_totalDuration = wp.time;
        }
    }
}

void ComponentAnimation::Update(float deltaTime)
{
    if (!m_isPlaying || m_waypoints.empty() || m_totalDuration <= 0.0f)
        return;

    if (m_isReversing)
    {
        m_currentTime -= deltaTime;
        if (m_currentTime <= 0.0f)
        {
            if (m_pathMode == PathMode::REVERSE)
            {
                m_isReversing = false;
                m_currentTime = 0.0f;
            }
            else if (m_pathMode == PathMode::LOOP)
            {
                m_isReversing = false;
                m_currentTime = 0.0f;
            }
            else // STOP
            {
                m_isPlaying = false;
                m_currentTime = 0.0f;
            }
        }
    }
    else
    {
        m_currentTime += deltaTime;

        if (m_currentTime >= m_totalDuration)
        {
            if (m_pathMode == PathMode::LOOP)
            {
                m_currentTime = std::fmod(m_currentTime, m_totalDuration);
            }
            else if (m_pathMode == PathMode::REVERSE)
            {
                m_isReversing = true;
                m_currentTime = m_totalDuration;
            }
            else // STOP
            {
                m_isPlaying = false;
                m_currentTime = m_totalDuration;
            }
        }
    }
}

void ComponentAnimation::GetInterpolatedTransform(glm::vec3& outPosition, glm::vec3& outRotation, float& outAlpha) const
{
    outAlpha = 0.0f;

    if (m_waypoints.empty() || m_totalDuration <= 0.0f)
    {
        outPosition = glm::vec3(0.0f);
        outRotation = glm::vec3(0.0f);
        return;
    }

    // Normalize time [0, 1]
    float normalizedTime = glm::clamp(m_currentTime / m_totalDuration, 0.0f, 1.0f);

    // Find the current segment
    size_t segStart, segEnd;
    float localT;
    FindCurrentSegment(normalizedTime, segStart, segEnd, localT);

    // Interpolate between waypoints
    const Waypoint& wp0 = m_waypoints[segStart];
    const Waypoint& wp1 = m_waypoints[segEnd];

    // Apply easing
    float easedT = ApplyEasing(localT);

    // Interpolate position and rotation
    outPosition = InterpolatePosition(wp0.position, wp1.position, easedT);
    outRotation = InterpolateRotation(wp0.rotation, wp1.rotation, easedT);
    outAlpha = easedT;
}

void ComponentAnimation::FindCurrentSegment(float normalizedTime, size_t& outSegmentStart, size_t& outSegmentEnd, float& outLocalT) const
{
    // Handle edge cases
    if (normalizedTime <= 0.0f)
    {
        outSegmentStart = 0;
        outSegmentEnd = m_waypoints.size() > 1 ? 1 : 0;
        outLocalT = 0.0f;
        return;
    }

    if (normalizedTime >= 1.0f)
    {
        outSegmentEnd = m_waypoints.size() - 1;
        outSegmentStart = outSegmentEnd > 0 ? outSegmentEnd - 1 : 0;
        outLocalT = 1.0f;
        return;
    }

    // Find segment based on waypoint times
    float targetTime = normalizedTime * m_totalDuration;

    for (size_t i = 0; i < m_waypoints.size() - 1; ++i)
    {
        if (targetTime >= m_waypoints[i].time && targetTime <= m_waypoints[i + 1].time)
        {
            outSegmentStart = i;
            outSegmentEnd = i + 1;

            float timeDiff = m_waypoints[i + 1].time - m_waypoints[i].time;
            if (timeDiff > 0.0f)
            {
                outLocalT = (targetTime - m_waypoints[i].time) / timeDiff;
            }
            else
            {
                outLocalT = 0.0f;
            }
            return;
        }
    }

    // Fallback
    outSegmentStart = m_waypoints.size() - 2;
    outSegmentEnd = m_waypoints.size() - 1;
    outLocalT = 1.0f;
}

glm::vec3 ComponentAnimation::InterpolatePosition(const glm::vec3& p0, const glm::vec3& p1, float t) const
{
    return glm::mix(p0, p1, t);
}

glm::vec3 ComponentAnimation::InterpolateRotation(const glm::vec3& r0, const glm::vec3& r1, float t) const
{
    // Linear interpolation for Euler angles (suitable for smooth animation)
    return glm::mix(r0, r1, t);
}

float ComponentAnimation::ApplyEasing(float t) const
{
    switch (m_easingType)
    {
    case EasingType::LINEAR:
        return t;

    case EasingType::SMOOTHSTEP:
        // Smoothstep: 3t^2 - 2t^3
        return t * t * (3.0f - 2.0f * t);

    default:
        return t;
    }
}