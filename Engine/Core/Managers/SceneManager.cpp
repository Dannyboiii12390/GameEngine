#include "SceneManager.h"
#include <iostream>
#include "../../DebugUtils.h"

SceneManager& SceneManager::Instance()
{
	static SceneManager instance;
	return instance;
}

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

void SceneManager::RequestReplaceScene(std::unique_ptr<IScene>&& scene)
{
	// Queue the scene; don't destroy or swap now (unsafe mid-frame)
	m_pendingScene = std::move(scene);
}

void SceneManager::ApplyPending()
{
	if (!m_pendingScene)
		return;

	// Destroy current scene resources (safe because we call this after render/present)
	if (!m_scenes.empty())
	{
		try
		{
			m_scenes.top()->Stop();
			m_scenes.top()->Destroy();
		}
		catch (...)
		{
			LOG_DEBUG("Exception during scene Destroy() in ApplyPending");
		}
		m_scenes.pop();
	}

	m_scenes.push(std::move(m_pendingScene));

	// Critical: initialize the newly activated scene
	if (!m_scenes.empty() && m_scenes.top())
	{
		m_scenes.top()->Start(0.0f);
	}
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
