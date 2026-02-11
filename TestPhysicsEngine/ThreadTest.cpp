#include "pch.h"

#include "../PhysicsEngine/Threading/Thread.h"

#include <atomic>
#include <future>
#include <chrono>
#include <thread>
namespace ThreadTests
{
	using namespace Threading;

	TEST(ThreadTest, StartAndJoin_ExecutesFunction)
	{
		std::atomic<int> counter{ 0 };
		Thread t([](std::atomic<int>& c) { c.fetch_add(1, std::memory_order_relaxed); }, std::ref(counter));
		EXPECT_TRUE(t.isJoinable());
		t.Join();
		EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
	}

	TEST(ThreadTest, Detach_ExecutesFunction)
	{
		std::promise<int> p;
		auto fut = p.get_future();

		Thread t([pr = std::move(p)]() mutable {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			pr.set_value(42);
			});
		t.Detach();
		EXPECT_FALSE(t.isJoinable());

		auto status = fut.wait_for(std::chrono::milliseconds(200));
		EXPECT_EQ(status, std::future_status::ready);
		EXPECT_EQ(fut.get(), 42);
	}

	TEST(ThreadTest, MoveConstruct_TransfersThread)
	{
		std::atomic<int> counter{ 0 };
		Thread t1([](std::atomic<int>& c) { c.fetch_add(1, std::memory_order_relaxed); }, std::ref(counter));

		Thread t2(std::move(t1));
		EXPECT_FALSE(t1.isJoinable());
		EXPECT_TRUE(t2.isJoinable());

		t2.Join();
		EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
	}

	TEST(ThreadTest, MoveAssign_TransfersThread)
	{
		std::atomic<int> counter{ 0 };
		Thread t1([](std::atomic<int>& c) { c.fetch_add(1, std::memory_order_relaxed); }, std::ref(counter));
		Thread t2;
		t2 = std::move(t1);

		EXPECT_FALSE(t1.isJoinable());
		EXPECT_TRUE(t2.isJoinable());

		t2.Join();
		EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
	}

	TEST(ThreadTest, Start_WhenAlreadyRunning_Throws)
	{
		Thread t([]() { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
		EXPECT_THROW(t.Start([]() {}), std::runtime_error);
		t.Join(); // cleanup
	}

	TEST(ThreadTest, Destructor_JoinsThread)
	{
		std::atomic<int> counter{ 0 };
		{
			Thread t([](std::atomic<int>& c) {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				c.fetch_add(1, std::memory_order_relaxed);
				}, std::ref(counter));
			// don't call Join(); destructor should join
		}
		// after scope the destructor joined the thread
		EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
	}
}