#include "pch.h"

#include "../PhysicsEngine/Maths/Transform.h"
#include <glm/glm.hpp>

namespace RotationTests
{
	using Physics::Transform;

	constexpr float EPS = 1e-6f;

	TEST(RotationTest, SingleAxis90)
	{
		Transform t(glm::vec3(0.0f));
		t.ApplyAngularDisplacement(glm::vec3(90.0f, 0.0f, 0.0f));
		EXPECT_NEAR(t.GetRotationDegrees().x, 90.0f, EPS);
		EXPECT_NEAR(t.GetRotationDegrees().y, 0.0f, EPS);
		EXPECT_NEAR(t.GetRotationDegrees().z, 0.0f, EPS);
	}

	TEST(RotationTest, SingleAxis180)
	{
		Transform t;
		t.ApplyAngularDisplacement(glm::vec3(180.0f, 0.0f, 0.0f));
		EXPECT_NEAR(t.GetRotationDegrees().x, 180.0f, EPS);
	}

	TEST(RotationTest, SingleAxis270)
	{
		Transform t;
		t.ApplyAngularDisplacement(glm::vec3(270.0f, 0.0f, 0.0f));
		EXPECT_NEAR(t.GetRotationDegrees().x, 270.0f, EPS);
	}

	TEST(RotationTest, SingleAxis360WrapsToZero)
	{
		Transform t;
		t.ApplyAngularDisplacement(glm::vec3(360.0f, 0.0f, 0.0f));
		EXPECT_NEAR(t.GetRotationDegrees().x, 0.0f, EPS);
	}

	TEST(RotationTest, EachAxisTest)
	{
		Transform t;
		t.ApplyAngularDisplacement(glm::vec3(90.0f, 180.0f, 270.0f));
		EXPECT_NEAR(t.GetRotationDegrees().x, 90.0f, EPS);
		EXPECT_NEAR(t.GetRotationDegrees().y, 180.0f, EPS);
		EXPECT_NEAR(t.GetRotationDegrees().z, 270.0f, EPS);
	}

	TEST(RotationTest, CombinedOverMultipleApplies)
	{
		Transform t;
		t.ApplyAngularDisplacement(glm::vec3(45.0f, 0.0f, 0.0f));
		t.ApplyAngularDisplacement(glm::vec3(45.0f, 0.0f, 0.0f)); // now 90
		EXPECT_NEAR(t.GetRotationDegrees().x, 90.0f, EPS);

		// add 270 -> total 360 -> wraps to 0
		t.ApplyAngularDisplacement(glm::vec3(270.0f, 0.0f, 0.0f));
		EXPECT_NEAR(t.GetRotationDegrees().x, 0.0f, EPS);
	}

	TEST(RotationTest, NegativeDeltaAndNormalization)
	{
		Transform t(glm::vec3(10.0f, 20.0f, 30.0f));
		t.ApplyAngularDisplacement(glm::vec3(-20.0f, -40.0f, -60.0f));
		// expected normalized: 350, 340, 330
		EXPECT_NEAR(t.GetRotationDegrees().x, 350.0f, EPS);
		EXPECT_NEAR(t.GetRotationDegrees().y, 340.0f, EPS);
		EXPECT_NEAR(t.GetRotationDegrees().z, 330.0f, EPS);
	}
}