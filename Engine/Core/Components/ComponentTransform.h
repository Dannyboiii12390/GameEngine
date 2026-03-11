#pragma once

#include "IComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
	glm::vec3 Position;
	glm::vec3 Rotation;
	glm::vec3 Scale;
	Transform(const glm::vec3& position = glm::vec3(0.0f), const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f))
		: Position(position), Rotation(rotation), Scale(scale) { }
};

class ComponentTransform : public IComponent
{
public:
	ComponentTransform(const glm::vec3& position = glm::vec3(0.0f), const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f))
		: IComponent(EComponentType::Component_Transform)
		, m_Transforms{ Transform(position, rotation, scale), Transform(position, rotation, scale) }
		, m_ReadBuffer(&m_Transforms[0])
		, m_WriteBuffer(&m_Transforms[1])
		, m_PreviousPosition(position)
	{ }

	// -----------------------------------------------------------------
	// Double-buffer access
	// Physics reads the committed state from the read buffer and writes
	// integrated results into the write buffer. Call SwapBuffers() once
	// per frame (after integration, before rendering) to promote the
	// write buffer into the read buffer.
	// -----------------------------------------------------------------
	const Transform* ReadBuffer()  const { return m_ReadBuffer;  }
	Transform*       WriteBuffer()       { return m_WriteBuffer; }

	// Swap read <-> write pointers. Call this after the physics/velocity
	// systems have finished writing for the current frame.
	void SwapBuffers()
	{
		std::swap(m_ReadBuffer, m_WriteBuffer);
	}

	// -----------------------------------------------------------------
	// Transform matrix is built from the read buffer (committed state).
	// -----------------------------------------------------------------
	glm::mat4 GetTransformMatrix() const
	{
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), m_ReadBuffer->Position);
		glm::mat4 rotationX   = glm::rotate(glm::mat4(1.0f), glm::radians(m_ReadBuffer->Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rotationY   = glm::rotate(glm::mat4(1.0f), glm::radians(m_ReadBuffer->Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 rotationZ   = glm::rotate(glm::mat4(1.0f), glm::radians(m_ReadBuffer->Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 rotation    = rotationZ * rotationY * rotationX;
		glm::mat4 scale       = glm::scale(glm::mat4(1.0f), m_ReadBuffer->Scale);

		return translation * rotation * scale;
	}

	// Read accessors always pull from the read (committed) buffer.
	const glm::vec3& Position() const { return m_ReadBuffer->Position; }
	const glm::vec3& Rotation() const { return m_ReadBuffer->Rotation; }
	const glm::vec3& Scale()    const { return m_ReadBuffer->Scale;    }

	// Previous-position helpers
	// Call SavePreviousPosition() before applying the per-frame positional integration so the previous
	// (pre-move) position is available for collision resolution.
	void SavePreviousPosition() { m_PreviousPosition = m_ReadBuffer->Position; }
	const glm::vec3& PreviousPosition() const { return m_PreviousPosition; }

	// Write setters target the write buffer so the read buffer remains
	// stable while physics is integrating.
	void SetPosition(const glm::vec3& position) { m_WriteBuffer->Position = position; }
	void SetRotation(const glm::vec3& rotation) { m_WriteBuffer->Rotation = rotation; }
	void SetScale   (const glm::vec3& scale)    { m_WriteBuffer->Scale    = scale;    }

	// Delta helpers seed from the read buffer so the write buffer always
	// starts from the last committed state.
	void ChangePosition(const glm::vec3& delta) { m_WriteBuffer->Position = m_ReadBuffer->Position + delta; }
	void ChangeRotation(const glm::vec3& delta) { m_WriteBuffer->Rotation = m_ReadBuffer->Rotation + delta; }
	void ChangeScale   (const glm::vec3& delta) { m_WriteBuffer->Scale    = m_ReadBuffer->Scale    + delta; }

private:

	// Two Transform value buffers. Pointers below select which is read/write this frame.
	Transform  m_Transforms[2];
	Transform* m_ReadBuffer;   // physics reads committed state from here
	Transform* m_WriteBuffer;  // physics writes integrated state here

	// store previous-frame position so collision response can restore pre-penetration transform
	glm::vec3 m_PreviousPosition;
};