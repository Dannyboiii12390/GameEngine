#pragma once

#include "IComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class ComponentTranslation : public IComponent
{
public:
	ComponentTranslation(const glm::vec3& position = glm::vec3(0.0f), const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f)) : IComponent(EComponentType::Component_Translation), m_Position(position), m_Rotation(rotation), m_Scale(scale) { }

	glm::mat4 GetTransformMatrix() const
	{
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), m_Position);
		glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 rotation = rotationZ * rotationY * rotationX;
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), m_Scale);
		return translation * rotation * scale;
	}
	const glm::vec3& Position() const { return m_Position; }
	const glm::vec3& Rotation() const { return m_Rotation; }
	const glm::vec3& Scale() const { return m_Scale; }

	void SetRotation(const glm::vec3& rotation) { m_Rotation = rotation; }
	void SetPosition(const glm::vec3& position) { m_Position = position; }
	void SetScale(const glm::vec3& scale) { m_Scale = scale; }

	void ChangePosition(const glm::vec3& delta) { m_Position += delta; }
	void ChangeRotation(const glm::vec3& delta) { m_Rotation += delta; }
	void ChangeScale(const glm::vec3& delta) { m_Scale += delta; }


private:

	glm::vec3 m_Position;
	glm::vec3 m_Rotation;
	glm::vec3 m_Scale;

};