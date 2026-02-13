#pragma once


/* things still to inherit and implement Collider
LineInf
*/

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

		virtual bool isColliding(const Sphere& other) const = 0;
		virtual bool isColliding(const LineInf& other) const = 0;
		virtual bool isColliding(const Capsule& other) const = 0;
		virtual bool isColliding(const Cylinder& other) const = 0;
		virtual bool isColliding(const Plane& other) const = 0;
	private:
		EColliderType m_type = EColliderType::NONE;
	};
}
