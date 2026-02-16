/* PSEUDOCODE / PLAN:
   - Resolve ambiguous type lookup for collider type names by using fully-qualified names.
   - Keep forward declarations for all shape types inside the Physics namespace.
   - Change all virtual method signatures to accept ::Physics::<Type> references to avoid ambiguity when other symbols named Sphere/Plane/etc exist in other scopes.
   - Ensure the static dispatcher compiles: use fully-qualified types in static_casts and provide balanced control flow.
   - Keep the rest of the class API unchanged.
*/

#pragma once

namespace Physics
{
	class Sphere;
	class LineInf;
	class Capsule;
	class Cylinder;
	class Plane;

	enum class EColliderType
	{
		NONE,
		SPHERE,
		LINEINF,
		CAPSULE,
		CYLINDER,
		PLANE
	};

	class Collider
	{
	public: 
		Collider(EColliderType p_type) : m_type(p_type) {};
		virtual ~Collider() = default; 

		// Use fully-qualified names to avoid ambiguous lookup (e.g. other Sphere symbols in other namespaces).
		virtual bool isColliding(const ::Physics::Sphere& other) const = 0;
		virtual bool isColliding(const ::Physics::LineInf& other) const = 0;
		virtual bool isColliding(const ::Physics::Capsule& other) const = 0;
		virtual bool isColliding(const ::Physics::Cylinder& other) const = 0;
		virtual bool isColliding(const ::Physics::Plane& other) const = 0;
		virtual void setPosition(const glm::vec3& newPos) {} // Only relevant for some collider types, so provide a default empty implementation.

		EColliderType getType() const { return m_type; }
	private:
		EColliderType m_type = EColliderType::NONE;
	};
}
