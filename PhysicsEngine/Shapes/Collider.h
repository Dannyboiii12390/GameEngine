

#pragma once
#define GLM_ENABLE_EXPERIMENTAL

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

		virtual void setPosition(const glm::vec3& newPos) {}
		virtual void setRotation(const glm::vec3& newRot) {}
		virtual void setScale(const glm::vec3& newScale) {}



		virtual bool isColliding(const Collider& other) const = 0;

		EColliderType getType() const { return m_type; }
	private:
		EColliderType m_type = EColliderType::NONE;
	};
}
