#include "SystemManager.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <Windows.h>
#include <algorithm>

namespace
{
	enum class ThreadRole
	{
		Graphics,
		Simulation
	};

	DWORD_PTR BuildMaskFromRange(uint32_t start, uint32_t endInclusive, uint32_t coreCount)
	{
		if (coreCount == 0)
			return 0;

		start = std::min(start, coreCount - 1);
		endInclusive = std::min(endInclusive, coreCount - 1);

		if (start > endInclusive)
			return 0;

		DWORD_PTR mask = 0;
		for (uint32_t i = start; i <= endInclusive; ++i)
		{
			mask |= (static_cast<DWORD_PTR>(1) << i);
		}
		return mask;
	}

	void ApplyThreadAffinity(ThreadRole role)
	{
		const uint32_t coreCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		DWORD_PTR mask = 0;

		switch (role)
		{
		case ThreadRole::Graphics:
			mask = BuildMaskFromRange(0, 0, coreCount);
			break;
		case ThreadRole::Simulation:
			mask = BuildMaskFromRange(3, coreCount > 0 ? coreCount - 1 : 0, coreCount);
			break;
		}

		if (mask == 0)
		{
			mask = BuildMaskFromRange(0, coreCount > 0 ? coreCount - 1 : 0, coreCount);
		}

		if (mask != 0)
		{
			SetThreadAffinityMask(GetCurrentThread(), mask);
		}
	}
}

void SystemManager::Update(std::span<Entity> entities, float dt)
{
	for (auto& system : m_systems)
	{
		system->OnUpdate(entities, dt);
	}
}

void SystemManager::StartSimulationThread(std::vector<Entity>& entities,
	std::mutex& sceneMutex,
	std::atomic<bool>& paused,
	std::atomic<int>& physicsHz)
{
	if (m_simulationRunning.load())
		return;

	m_simulationRunning.store(true);
	m_simulationThread = std::thread([this, &entities, &sceneMutex, &paused, &physicsHz]()
		{
			ApplyThreadAffinity(ThreadRole::Simulation);

			while (m_simulationRunning.load())
			{
				const auto frameStart = std::chrono::steady_clock::now();
				const int targetHz = std::max(1, physicsHz.load());
				const auto tick = std::chrono::duration<double>(1.0 / static_cast<double>(targetHz));
				float dt = static_cast<float>(tick.count());

				if (paused.load())
					dt = 0.0f;

				{
					std::lock_guard<std::mutex> lock(sceneMutex);
					Update(entities, dt);
				}

				std::this_thread::sleep_until(frameStart + tick);
			}
		});
}

void SystemManager::StartGraphicsThread(std::function<void()> renderCallback,
	std::atomic<int>& graphicsHz)
{
	if (m_graphicsRunning.load())
		return;

	m_graphicsRunning.store(true);
	m_graphicsThread = std::thread([this, renderCallback, &graphicsHz]()
		{
			ApplyThreadAffinity(ThreadRole::Graphics);
			m_graphicsThreadId = std::this_thread::get_id();

			while (m_graphicsRunning.load())
			{
				const auto frameStart = std::chrono::steady_clock::now();
				const int targetHz = std::max(1, graphicsHz.load());
				const auto tick = std::chrono::duration<double>(1.0 / static_cast<double>(targetHz));

				if (renderCallback)
					renderCallback();

				std::this_thread::sleep_until(frameStart + tick);
			}
		});
}

void SystemManager::StopThreads()
{
	m_simulationRunning.store(false);
	if (m_simulationThread.joinable())
	{
		m_simulationThread.join();
	}

	m_graphicsRunning.store(false);
	if (m_graphicsThread.joinable())
	{
		m_graphicsThread.join();
	}
}