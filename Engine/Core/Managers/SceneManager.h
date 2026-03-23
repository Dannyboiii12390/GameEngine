#pragma once

#include <stack>
#include <memory>
#include "../Scenes/IScene.h"

class IScene;

class SceneManager
{
public:
	SceneManager() = default;
	~SceneManager() = default;

	// Singleton accessor for global access from scenes/menus
	static SceneManager& Instance();

	void AddScene(std::unique_ptr<IScene>&& scene);
	std::unique_ptr<IScene> PopScene();
	IScene* GetCurrentScene() const;
	void Shutdown();

	// Enqueue a scene replacement to be applied safely later (after current frame)
	void RequestReplaceScene(std::unique_ptr<IScene>&& scene);

	// Apply any pending replacement. Call from main loop after rendering/present.
	void ApplyPending();

private:
	std::stack<std::unique_ptr<IScene>> m_scenes;
	std::unique_ptr<IScene> m_pendingScene;
};

