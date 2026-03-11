#pragma once

#include <vector>
#include <memory>
#include <span>
#include <vulkan/vulkan.h>

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

	void Shutdown()
	{
		m_renderer.Shutdown();
	}

private:
	SystemRenderer m_renderer;
	std::vector<std::unique_ptr<ISystem>> m_systems;
	const unsigned int physicsFramesBeforeNextRender;
};
