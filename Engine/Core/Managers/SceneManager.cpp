#include "SceneManager.h"
#include <iostream>
#include "../../DebugUtils.h"

void SceneManager::AddScene(std::unique_ptr<IScene>&& scene)
{
	m_scenes.push(std::move(scene));
}
std::unique_ptr<IScene> SceneManager::PopScene()
{
	if (m_scenes.empty())
		return nullptr;
	std::unique_ptr<IScene> topScene = std::move(m_scenes.top());
	m_scenes.pop();
	return topScene;
}
IScene* SceneManager::GetCurrentScene() const
{
	if (m_scenes.empty())
		return nullptr;
	return m_scenes.top().get();
}

void SceneManager::Shutdown()
{
	LOG_DEBUG("Number of scenes to shutdown: " << m_scenes.size());

	while (!m_scenes.empty())
	{
		m_scenes.top()->Destroy(); // Call Destroy on the scene to clean up its resources
		m_scenes.pop();
	}
}