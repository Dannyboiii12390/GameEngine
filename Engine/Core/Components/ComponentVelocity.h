#pragma once

#include "IComponent.h"
#include <glm/glm.hpp>
#include <algorithm> // std::swap

struct Velocity
{
	glm::vec3 PositionalVelocity;
	glm::vec3 RotationalVelocity;
	glm::vec3 ScalarVelocity;

	Velocity(const glm::vec3& posVel = glm::vec3(0.0f), const glm::vec3& rotVel = glm::vec3(0.0f), const glm::vec3& scalarVel = glm::vec3(1.0f))
		: PositionalVelocity(posVel), RotationalVelocity(rotVel), ScalarVelocity(scalarVel) { }
};

class ComponentVelocity : public IComponent
{
public:
	ComponentVelocity(const glm::vec3& posVel = glm::vec3(0.0f), const glm::vec3& rotVel = glm::vec3(0.0f), const glm::vec3& scalarVel = glm::vec3(1.0f))
		: IComponent(EComponentType::Component_Velocity)
		, m_Velocities{ Velocity(posVel, rotVel, scalarVel), Velocity(posVel, rotVel, scalarVel) }
		, m_ReadBuffer(&m_Velocities[0])
		, m_WriteBuffer(&m_Velocities[1])
	{ }

	// -----------------------------------------------------------------
	// Double-buffer access
	// Systems read the committed state from the read buffer and write
	// integrated results into the write buffer. Call SwapBuffers() once
	// per frame (after integration) to promote the write buffer into
	// the read buffer.
	// -----------------------------------------------------------------
	const Velocity* ReadBuffer()  const { return m_ReadBuffer;  }
	Velocity*       WriteBuffer()       { return m_WriteBuffer; }

	// Swap read <-> write pointers. Call this after the physics/velocity
	// systems have finished writing for the current frame.
	void SwapBuffers()
	{
		std::swap(m_ReadBuffer, m_WriteBuffer);
	}

	// Read accessors always pull from the read (committed) buffer.
	const glm::vec3& GetPositionVelocity()   const { return m_ReadBuffer->PositionalVelocity; }
	const glm::vec3& GetRotationalVelocity() const { return m_ReadBuffer->RotationalVelocity; }
	const glm::vec3& GetScaleVelocity()      const { return m_ReadBuffer->ScalarVelocity;     }

	// Write setters target the write buffer so the read buffer remains
	// stable while systems are integrating.
	void SetPositionalVelocity(const glm::vec3& velocity) { m_WriteBuffer->PositionalVelocity = velocity; }
	void SetRotationalVelocity(const glm::vec3& velocity) { m_WriteBuffer->RotationalVelocity = velocity; }
	void SetScalarVelocity    (const glm::vec3& velocity) { m_WriteBuffer->ScalarVelocity     = velocity; }

	// Delta helpers seed from the read buffer so the write buffer always
	// starts from the last committed state.
	void ChangePositionalVelocity(const glm::vec3& delta) { m_WriteBuffer->PositionalVelocity = m_ReadBuffer->PositionalVelocity + delta; }
	void ChangeRotationalVelocity(const glm::vec3& delta) { m_WriteBuffer->RotationalVelocity = m_ReadBuffer->RotationalVelocity + delta; }
	void ChangeScalarVelocity    (const glm::vec3& delta) { m_WriteBuffer->ScalarVelocity     = m_ReadBuffer->ScalarVelocity     + delta; }

private:

	// Two Velocity value buffers. Pointers below select which is read/write this frame.
	Velocity  m_Velocities[2];
	Velocity* m_ReadBuffer;   // systems read committed state from here
	Velocity* m_WriteBuffer;  // systems write integrated state here
};