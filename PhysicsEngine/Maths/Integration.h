#pragma once
#include <utility>

namespace Physics
{
	enum class EIntegrationMethod
	{
		Euler,
		SemiImplicitEuler,
		RungeKutta4,
		Midpoint
	};;

	template<typename T>
	class Integration
	{
	public:
		template<typename... Args>
		static T Integrate(EIntegrationMethod method, Args... args)
		{
			switch (method)
			{
			case EIntegrationMethod::Euler:
				return Euler(std::forward<Args>(args)...);
			case EIntegrationMethod::SemiImplicitEuler:
				return SemiImplicitEuler(std::forward<Args>(args)...);
			/*case EIntegrationMethod::RungeKutta4:
				return RungeKutta4(std::forward<Args>(args)...);
			case EIntegrationMethod::Midpoint:
				return Midpoint(std::forward<Args>(args)...);*/

			}
		}

		// Explicit (forward) Euler uses the current velocity to update position: x_{n+1} = x_n + v_n * dt.
		// Semi‑implicit(symplectic) Euler updates velocity first, then uses the updated velocity to update 
		// position : v_{ n + 1 } = v_n + a_n * dt, then x_{ n + 1 } = x_n + v_{ n + 1 } *dt.
		
		// Basic explicit Euler: x_{n+1} = x_n + dt * f(x_n)
		static T Euler(T current, T derivative, float deltaTime)
		{
			return current + derivative * deltaTime;
		}

		// Semi-implicit (a.k.a. Symplectic) Euler:
		// For a single value it is the same algebraic update as explicit Euler.
		// In typical physics usage this is used as:
		//   v_{n+1} = v_n + a_n * dt
		//   x_{n+1} = x_n + v_{n+1} * dt
		// Here we implement the single-step update (v_{n+1} or x_{n+1} depending on how it's used).
		static T SemiImplicitEuler(T current, T derivative, float deltaTime)
		{
			return current + derivative * deltaTime;
		}

		// Runge-Kutta 4 combination step:
		// x_{n+1} = x_n + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
		// Where derivative1..4 correspond to the k1..k4 terms (computed externally).
		static T RungeKutta4(T current, T derivative1, T derivative2, T derivative3, T derivative4, float deltaTime)
		{
			float dtOver6 = deltaTime / 6.0f;
			return current + (derivative1 + derivative4 + (derivative2 + derivative3) * 2.0f) * dtOver6;
		}

		// Midpoint (second-order Runge-Kutta / RK2):
		// Uses derivative at t (k1) and derivative at midpoint (k2).
		// x_{n+1} = x_n + dt * k2
		// Caller must supply both derivatives (k1, k2).
		static T Midpoint(T current, T derivative1, T derivative2, float deltaTime)
		{
			(void)derivative1; // k1 is provided for consistency with common RK2 usage; k2 is used for the step.
			return current + derivative2 * deltaTime;
		}
	
	};
}