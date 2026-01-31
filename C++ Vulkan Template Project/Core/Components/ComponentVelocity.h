#pragma once

#include "IComponent.h"
#include <glm/glm.hpp>


class ComponentVelocity : public IComponent
{
public:
	ComponentVelocity(const glm::vec3& posVel = glm::vec3(0.0f), const glm::vec3& rotVel = glm::vec3(0.0f), const glm::vec3& scalarVel = glm::vec3(1.0f)) : 
		m_PositionalVelocity(posVel), m_RotationalVelocity(rotVel), m_ScalarVelocity(scalarVel), 
		IComponent(EComponentType::Component_Velocity) { }

	const glm::vec3& GetPositionVelocity() const { return m_PositionalVelocity; }
	const glm::vec3& GetRotationalVelocity() const { return m_RotationalVelocity; }
	const glm::vec3& GetScaleVelocity() const { return m_ScalarVelocity; }

	void SetPositionalVelocity(const glm::vec3& velocity) { m_PositionalVelocity = velocity; }
	void SetRotationalVelocity(const glm::vec3& velocity) { m_RotationalVelocity = velocity; }
	void SetScalarVelocity(const glm::vec3& velocity) { m_ScalarVelocity = velocity; }

	void ChangePositionalVelocity(const glm::vec3& delta) { m_PositionalVelocity += delta; }
	void ChangeRotationalVelocity(const glm::vec3& delta) { m_RotationalVelocity += delta; }
	void ChangeScalarVelocity(const glm::vec3& delta) { m_ScalarVelocity += delta; }

private:
	glm::vec3 m_PositionalVelocity;
	glm::vec3 m_RotationalVelocity;
	glm::vec3 m_ScalarVelocity;

};