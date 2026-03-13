#include "Plane.h"
#include "Sphere.h"
#include "LineInf.h"
#include "Capsule.h"
#include "Cylinder.h"
#include <glm/gtx/rotate_vector.hpp>


using namespace Physics;

Plane::Plane(const glm::vec3& point, const glm::vec3& u, const glm::vec3& v)
    : m_point(point), m_u(u), m_v(v), Collider(EColliderType::PLANE),
      m_basePoint(point), m_baseU(u), m_baseV(v)
{
    m_normal = glm::cross(m_u, m_v);
    m_unitNormal = glm::normalize(m_normal);

    // snap tiny values to zero to avoid confusing debugger output
    auto snap = [](glm::vec3& v, float eps = 1e-6f) {
        v.x = (std::abs(v.x) < eps) ? 0.0f : v.x;
        v.y = (std::abs(v.y) < eps) ? 0.0f : v.y;
        v.z = (std::abs(v.z) < eps) ? 0.0f : v.z;
    };
    snap(m_u); snap(m_v); snap(m_normal); snap(m_unitNormal);
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

bool Plane::isColliding(const Sphere& other) const
{
	// Collision when sphere center's absolute signed distance to plane <= sphere radius
	float d = signedDistance(other.getPos());
	return std::abs(d) <= other.getRadius();
}
bool Plane::isColliding(const LineInf& other) const
{
	// Infinite line intersects plane if it is not parallel, or if it lies in the plane.
	const glm::vec3 A = other.getA();
	const glm::vec3 B = other.getB();

	const float EPS = 1e-6f;

	float dA = signedDistance(A);
	float dB = signedDistance(B);

	// If either endpoint lies (numerically) on the plane -> intersects
	if (std::abs(dA) <= EPS || std::abs(dB) <= EPS)
		return true;

	glm::vec3 dir = B - A;
	float denom = glm::dot(m_unitNormal, dir);

	// If line direction is (nearly) orthogonal to plane normal -> parallel to plane
	if (std::abs(denom) <= EPS)
	{
		// Parallel: either entire line lies in the plane (dA ~= 0) or no intersection.
		return false;
	}

	// Non-parallel infinite line always has a solution t where it crosses the plane.
	return true;
}
bool Plane::isColliding(const Capsule& other) const
{
	// Capsule collides with plane if the shortest distance from the capsule's segment
	// to the plane is <= capsule radius. Equivalently: if segment crosses the plane,
	// or either endpoint is within radius distance of the plane.
	const glm::vec3 A = other.getA();
	const glm::vec3 B = other.getB();
	const float r = other.getRadius();

	const float EPS = 1e-6f;

	float dA = signedDistance(A);
	float dB = signedDistance(B);

	// Degenerate segment -> treat as sphere at A with radius r
	if (glm::length(B - A) <= EPS)
		return std::abs(dA) <= r;

	// If either cap center is within radius of plane -> collision
	if (std::abs(dA) <= r || std::abs(dB) <= r)
		return true;

	// If segment crosses plane (signed distances opposite signs) -> collision
	if (dA * dB < 0.0f)
		return true;

	// Otherwise entire capsule is on one side and farther than radius -> no collision
	return false;
}
bool Plane::isColliding(const Cylinder& other) const
{
	// Finite cylinder collides with plane if the minimal distance from its axis segment
	// (including end caps) to the plane is <= cylinder radius.
	const glm::vec3 A = other.getA();
	const glm::vec3 B = other.getB();
	const float cylR = other.getRadius();

	const float EPS = 1e-6f;

	float dA = signedDistance(A);
	float dB = signedDistance(B);

	// Degenerate axis -> treat as disk/sphere at A with radius cylR
	if (glm::length(B - A) <= EPS)
		return std::abs(dA) <= cylR;

	// If either cap center is within radius distance -> intersection
	if (std::abs(dA) <= cylR || std::abs(dB) <= cylR)
		return true;

	// If axis segment crosses plane -> intersection
	if (dA * dB < 0.0f)
		return true;

	// Axis entirely on one side and both endpoints farther than radius -> no intersection
	return false;
}
bool Plane::isColliding(const Plane& other) const
{
	// Two infinite planes intersect unless they are parallel and distinct.
	const float EPS = 1e-6f;

	// Normals are available (same class) => check parallelism via cross product.
	glm::vec3 n1 = m_unitNormal;
	glm::vec3 n2 = other.m_unitNormal;
	glm::vec3 cr = glm::cross(n1, n2);

	// If normals are not collinear -> planes intersect along a line.
	if (glm::length(cr) > EPS)
		return true;

	// Parallel: check if they are coincident by testing distance from other's reference point to this plane.
	float d = signedDistance(other.m_point);
	return std::abs(d) <= EPS;
}
void Plane::setPosition(const glm::vec3& newPos)
{
	m_point = newPos;
}
void Plane::setRotation(const glm::vec3& eulerDegrees)
{
	glm::mat4 rot = glm::mat4(1.0f);
	rot = glm::rotate(rot, glm::radians(eulerDegrees.x), glm::vec3(1, 0, 0));
	rot = glm::rotate(rot, glm::radians(eulerDegrees.y), glm::vec3(0, 1, 0));
	rot = glm::rotate(rot, glm::radians(eulerDegrees.z), glm::vec3(0, 0, 1));

	// Only rotate the tangent vectors — the plane's world-space point stays fixed.
	m_u = glm::vec3(rot * glm::vec4(m_baseU, 0.0f));
	m_v = glm::vec3(rot * glm::vec4(m_baseV, 0.0f));
	m_point = m_basePoint; // position is managed by setPosition, not rotation

	m_normal = glm::cross(m_u, m_v);
	m_unitNormal = glm::normalize(m_normal);
}
