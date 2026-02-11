#include "pch.h"

#include "../PhysicsEngine/Threading/ThreadPool.h"

#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

namespace ThreadTests
{
	using namespace Threading;

	TEST(ThreadPoolTest, Enqueue_ReturnsResult)
	{
		ThreadPool pool(4);
		auto fut = pool.Enqueue([](int a) { return a + 1; }, 41);
		EXPECT_EQ(fut.get(), 42);
		pool.Shutdown();
	}

	TEST(ThreadPoolTest, Enqueue_VoidTask)
	{
		ThreadPool pool(2);
		std::atomic<int> counter{ 0 };
		auto fut = pool.Enqueue([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
		fut.get(); // wait
		EXPECT_EQ(counter.load(), 1);
		pool.Shutdown();
	}

	TEST(ThreadPoolTest, ExceptionPropagatesThroughFuture)
	{
		ThreadPool pool(2);
		auto fut = pool.Enqueue([]() -> int { throw std::runtime_error("boom"); });
		EXPECT_THROW(fut.get(), std::runtime_error);
		pool.Shutdown();
	}

	TEST(ThreadPoolTest, ManyTasks_Concurrently)
	{
		const int N = 200;
		ThreadPool pool(8);
		std::vector<std::future<int>> futures;
		futures.reserve(N);
		for (int i = 0; i < N; ++i)
		{
			futures.push_back(pool.Enqueue([i]() { return i + 1; }));
		}

		int sum = 0;
		for (auto& f : futures) sum += f.get();
		// sum of 1..N
		int expected = (N * (N + 1)) / 2;
		EXPECT_EQ(sum, expected);
		pool.Shutdown();
	}

	TEST(ThreadPoolTest, Shutdown_PreventsEnqueue)
	{
		ThreadPool pool(2);
		pool.Shutdown();
		EXPECT_THROW(pool.Enqueue([]() { return 1; }), std::runtime_error);
	}
}