#pragma once

#include <future>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Threading
{
	class ThreadPool
	{
	public:
		ThreadPool(unsigned int numThreads);
		~ThreadPool();

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		template<typename Func, typename... Args>
		auto Enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

		void Shutdown();

		size_t size() const { return m_workers.size(); }

	private:
		// Worker threads
		std::vector<std::thread> m_workers;

		// task queue: each task is a void() callable
		std::queue<std::function<void()>> m_taskQueue;
		mutable std::mutex m_queueMutex;
		std::condition_variable m_condition;
		std::atomic_bool m_stop = false;
	};
}

// Template implementation must live in the header so it is visible to all TUs.
template<typename Func, typename... Args>
auto Threading::ThreadPool::Enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
{
	using return_type = std::invoke_result_t<Func, Args...>;

	if (m_stop.load(std::memory_order_acquire))
		throw std::runtime_error("ThreadPool is stopped; cannot enqueue new tasks.");

	auto bound = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
	auto taskPtr = std::make_shared<std::packaged_task<return_type()>>(std::move(bound));
	std::future<return_type> res = taskPtr->get_future();

	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if (m_stop.load(std::memory_order_acquire))
			throw std::runtime_error("ThreadPool is stopped; cannot enqueue new tasks.");
		m_taskQueue.emplace([taskPtr]() { (*taskPtr)(); });
	}

	m_condition.notify_one();
	return res;
}