#pragma once

namespace Physics
{
	class Sphere;
	class LineInf;
	class Capsule;
	class Cylinder;
	class Plane;

	class Collider
	{
	public: 
		Collider() = default; 
		virtual ~Collider() = default; 

		virtual bool isColliding(const Sphere& other) const = 0;
		virtual bool isColliding(const LineInf& other) const = 0;
		virtual bool isColliding(const Capsule& other) const = 0;
		virtual bool isColliding(const Cylinder& other) const = 0;
		virtual bool isColliding(const Plane& other) const = 0;



	};
}
