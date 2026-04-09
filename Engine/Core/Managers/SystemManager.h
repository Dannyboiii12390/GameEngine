#pragma once

#include <vector>
#include <memory>
#include <span>
#include <vulkan/vulkan.h>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <mutex>

#include "../Systems/SystemRenderer.h"
#include "../Systems/ISystem.h"
#include "../Scenes/IScene.h"
#include "../../Renderer/VulkanRHI.h"

// Forward declarations
#include "../Entity.h"

class SystemManager
{
public:
	SystemManager(VulkanRHI* rhi, const int frames_between_renders = 1)
		: physicsFramesBeforeNextRender(frames_between_renders)
	{
		m_renderer.Initialize(rhi);
	}

	~SystemManager()
	{
		StopThreads();
	}

	void Update(std::span<Entity> entities, float deltaTime);

	void RegisterSystem(std::unique_ptr<ISystem> system)
	{
		// move the unique_ptr into the container (cannot copy)
		m_systems.push_back(std::move(system));
	}

	void Render(VkCommandBuffer cmd, std::vector<Entity>& entities)
	{
		m_renderer.Render(cmd, entities);
	}

	void UpdateAndRender(std::vector<Entity> entities, float deltaTime, VkCommandBuffer cmd)
	{
		static int physicsFrameCounter = 0;
		Update(entities, deltaTime);
		if (physicsFrameCounter >= physicsFramesBeforeNextRender)
		{
			Render(cmd, entities);
			physicsFrameCounter = 0;
		}
		else
		{
			physicsFrameCounter++;
		}
	}

	void StartSimulationThread(std::vector<Entity>& entities,
		std::mutex& sceneMutex,
		std::atomic<bool>& paused,
		std::atomic<int>& physicsHz);

	void StartGraphicsThread(std::function<void()> renderCallback,
		std::atomic<int>& graphicsHz);

	void StopThreads();

	bool IsSimulationThreadRunning() const { return m_simulationRunning.load(); }
	bool IsGraphicsThreadRunning() const { return m_graphicsRunning.load(); }
	std::thread::id GetGraphicsThreadId() const { return m_graphicsThreadId; }

	void Shutdown()
	{
		StopThreads();
		m_renderer.Shutdown();
	}

private:
	SystemRenderer m_renderer;
	std::vector<std::unique_ptr<ISystem>> m_systems;
	const unsigned int physicsFramesBeforeNextRender;

	std::thread m_simulationThread;
	std::thread m_graphicsThread;
	std::atomic<bool> m_simulationRunning{ false };
	std::atomic<bool> m_graphicsRunning{ false };
	std::thread::id m_graphicsThreadId{};
};
