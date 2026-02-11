#include "ThreadPool.h"

#include <functional>
#include <memory>
#include <stdexcept>

namespace Threading
{
	ThreadPool::ThreadPool(int numThreads)
	{
		if(numThreads <= 0)
			throw std::invalid_argument("Number of threads must be greater than 0");

		m_workers.reserve(numThreads);
		m_stop.store(false);

		for(size_t i = 0; i < static_cast<size_t>(numThreads); ++i)
		{
			m_workers.emplace_back([this]()
			{
				while(true)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(m_queueMutex);
						m_condition.wait(lock, [this]()
						{
							return m_stop.load() || !m_taskQueue.empty();
						});
						if(m_stop.load() && m_taskQueue.empty())
							return;
						task = std::move(m_taskQueue.front());
						m_taskQueue.pop();
					}
					task();
				}
			});
		}

	}
	ThreadPool::~ThreadPool()
	{
		Shutdown();
	}
	void ThreadPool::Shutdown()
	{
		m_stop.store(true);
		m_condition.notify_all();
		for(std::thread& worker : m_workers)
		{
			if(worker.joinable())
				worker.join();
		}
		m_workers.clear();
	}
}