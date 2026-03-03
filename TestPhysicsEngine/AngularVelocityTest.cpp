#include "pch.h"

#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace RotationTest
{
	// Helper to simulate angular velocity integration for a given axis vector of omega (radians/sec).
	// Deterministic: use integer step count + remainder, accumulate in double to reduce FP error.
	static glm::vec3 SimulateAngularVelocity(const glm::vec3& omegaRadPerSec, float seconds, float fixedDt)
	{
		int steps = static_cast<int>(seconds / fixedDt);               // floor(seconds / dt)
		double accX = 0.0, accY = 0.0, accZ = 0.0;

		for (int i = 0; i < steps; ++i)
		{
			accX += static_cast<double>(omegaRadPerSec.x) * static_cast<double>(fixedDt);
			accY += static_cast<double>(omegaRadPerSec.y) * static_cast<double>(fixedDt);
			accZ += static_cast<double>(omegaRadPerSec.z) * static_cast<double>(fixedDt);
		}

		double remainder = static_cast<double>(seconds) - static_cast<double>(steps) * static_cast<double>(fixedDt);
		if (remainder > 1e-12)
		{
			accX += static_cast<double>(omegaRadPerSec.x) * remainder;
			accY += static_cast<double>(omegaRadPerSec.y) * remainder;
			accZ += static_cast<double>(omegaRadPerSec.z) * remainder;
		}

		return glm::degrees(glm::vec3(static_cast<float>(accX), static_cast<float>(accY), static_cast<float>(accZ)));
	}

	// Normalize degrees into [0,360)
	static float NormalizeDeg(float d)
	{
		float r = std::fmod(d, 360.0f);
		if (r < 0.0f) r += 360.0f;
		// treat very-close-to-360 as 0
		if (std::abs(r - 360.0f) < 1e-6f) r = 0.0f;
		return r;
	}

	TEST(AngularVelocityTest, CardinalAxesAndCombination_FixedTimestepIntegration)
	{
		const float EPS = 1e-3f; // tolerance for floating point accumulation
		const float DT = 0.01f;  // fixed timestep (10 ms)
		const float PI = glm::pi<float>();

		// angular velocities to test (radians per second)
		std::vector<float> omegas = { PI / 2.0f, PI, 3.0f * PI / 2.0f, 2.0f * PI }; // pi/2, pi, 3pi/2, 2pi

		// durations to test (seconds)
		std::vector<int> durations = { 1, 2, 3, 4 };

		for (float omega : omegas)
		{
			for (int seconds : durations)
			{
				// X axis only
				{
					glm::vec3 omegaVec(omega, 0.0f, 0.0f);
					glm::vec3 finalDeg = SimulateAngularVelocity(omegaVec, static_cast<float>(seconds), DT);
					float expectedDeg = NormalizeDeg(glm::degrees(omega * static_cast<float>(seconds)));
					EXPECT_NEAR(NormalizeDeg(finalDeg.x), expectedDeg, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.y), 0.0f, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.z), 0.0f, EPS);
				}

				// Y axis only
				{
					glm::vec3 omegaVec(0.0f, omega, 0.0f);
					glm::vec3 finalDeg = SimulateAngularVelocity(omegaVec, static_cast<float>(seconds), DT);
					float expectedDeg = NormalizeDeg(glm::degrees(omega * static_cast<float>(seconds)));
					EXPECT_NEAR(NormalizeDeg(finalDeg.y), expectedDeg, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.x), 0.0f, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.z), 0.0f, EPS);
				}

				// Z axis only
				{
					glm::vec3 omegaVec(0.0f, 0.0f, omega);
					glm::vec3 finalDeg = SimulateAngularVelocity(omegaVec, static_cast<float>(seconds), DT);
					float expectedDeg = NormalizeDeg(glm::degrees(omega * static_cast<float>(seconds)));
					EXPECT_NEAR(NormalizeDeg(finalDeg.z), expectedDeg, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.x), 0.0f, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.y), 0.0f, EPS);
				}

				// Combination on all three axes: use different omegas for each axis (rotX = omega, rotY = 2*omega, rotZ = 3*omega)
				{
					glm::vec3 omegaVec(omega, 2.0f * omega, 3.0f * omega);
					glm::vec3 finalDeg = SimulateAngularVelocity(omegaVec, static_cast<float>(seconds), DT);
					float expectedX = NormalizeDeg(glm::degrees(omega * static_cast<float>(seconds)));
					float expectedY = NormalizeDeg(glm::degrees(2.0f * omega * static_cast<float>(seconds)));
					float expectedZ = NormalizeDeg(glm::degrees(3.0f * omega * static_cast<float>(seconds)));
					EXPECT_NEAR(NormalizeDeg(finalDeg.x), expectedX, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.y), expectedY, EPS);
					EXPECT_NEAR(NormalizeDeg(finalDeg.z), expectedZ, EPS);
				}
			}
		}
	}
}
