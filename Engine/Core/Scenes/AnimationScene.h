#pragma once

#include "IScene.h"
#include "../../Renderer/Window.h"
#include "../../Renderer/Camera.h"
#include "../../Renderer/GUI.h"
#include "../Entity.h"
#include "../InputHandler.h"
#include "../Managers/SystemManager.h"
#include <vector>
#include <glm/glm.hpp>
#include <chrono>

class VulkanRHI;

class AnimationScene : public IScene
{
public:
    AnimationScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui);
    virtual ~AnimationScene();

    virtual void Start(float deltaTime) override;
    virtual void Stop() override;
    virtual void Update(float deltaTime) override;
    virtual void FixedUpdate() override;
    virtual void Draw() override;
    virtual void HandleInput(float deltaTime) override;
    virtual void SerializeState() override;
    virtual void DeserializeState() override;
    virtual void Destroy() override;

    virtual void AddEntity(Entity&& entity) override;
    virtual void RemoveEntity(int index) override;

private:
    void CreateLinearPathObject();
    void CreateLoopingPathObject();
    void CreateReversingPathObject();
    void CreateSmoothstepObject();
    void CreateCollisionDemoObjects();

    // Scene members
    Window* m_window = nullptr;
    InputHandler m_inputHandler;
    Camera m_camera;
    VulkanRHI* m_vulkanRHI = nullptr;
    GUI* m_gui = nullptr;
    SystemManager m_systemManager;

    // Entity management
    std::vector<Entity> m_entities;

    // Timing and state
    float m_deltaTime = 0.0f;
    std::atomic<bool> m_paused{ false };
};