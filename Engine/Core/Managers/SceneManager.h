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

	void AddScene(std::unique_ptr<IScene>&& scene);
	std::unique_ptr<IScene> PopScene();
	IScene* GetCurrentScene() const;
	void Shutdown();


private:
	std::stack<std::unique_ptr<IScene>> m_scenes;
};

