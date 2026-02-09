
#include "Plane.h"

Plane::Plane(const glm::vec3& point, const glm::vec3& u, const glm::vec3& v) : m_point(point), m_u(u), m_v(v) 
{
	m_normal = glm::cross(m_u, m_v);
	m_unitNormal = glm::normalize(m_normal);
}
Plane Plane::FromThreePoints(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2)
{
	glm::vec3 u = p1 - p0;
	glm::vec3 v = p2 - p0;
	return Plane(p0, u, v);
}
glm::vec3 Plane::pointAt(float s, float t) const
{
	return m_point + s * m_u + t * m_v;
}
float Plane::signedDistance(const glm::vec3& p) const
{
	return glm::dot(p - m_point, m_unitNormal);
}
glm::vec3 Plane::projectPoint(const glm::vec3& p) const
{
	float d = signedDistance(p);
	return p - d * m_unitNormal;
}
bool Plane::containsPoint(const glm::vec3& p, float epsilon) const
{
	return glm::abs(signedDistance(p)) < epsilon;
}
float Plane::getShortestDistance(const glm::vec3& p) const
{
	return glm::abs(signedDistance(p));
}