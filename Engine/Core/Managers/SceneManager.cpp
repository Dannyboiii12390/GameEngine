#include "SceneManager.h"

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
	while (!m_scenes.empty())
	{
		m_scenes.pop();
	}
}