#pragma once
#include <concepts>
#include <thread>
#include <stdexcept>

namespace Threading
{
	class Thread
	{
	public:
		Thread() noexcept = default;

		template<typename Fn, typename... Args>
			requires std::invocable<Fn, Args...>
		explicit Thread(Fn&& fn, Args&&... args)
		{
			Start(std::forward<Fn>(fn), std::forward<Args>(args)...);
		}
		template<typename Fn, typename... Args>
			requires std::invocable<Fn, Args...>
		void Start(Fn&& fn, Args&&... args)
		{
			if(m_thread.joinable())
				throw std::runtime_error("Thread already running");

			m_thread = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
		}

		void Join();
		void Detach();

		bool isJoinable() const noexcept { return m_thread.joinable(); }
		std::thread::id GetId() const noexcept { return m_thread.get_id(); }
		std::thread::native_handle_type GetNativeHandle() { return m_thread.native_handle(); }

		// movable
		Thread(Thread&& other) noexcept : m_thread(std::move(other.m_thread)) {}
		Thread& operator=(Thread&& other) noexcept
		{
			if(this != &other)
			{
				if(m_thread.joinable())
					m_thread.join();
				m_thread = std::move(other.m_thread);
			}
			return *this;
		}

		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;

		~Thread()
		{
			if (m_thread.joinable())
			{
				try
				{
					m_thread.join();
				}
				catch (...)
				{
					// If join throws for some reason, terminate the program to avoid silent failure.
					std::terminate();
				}
			}
		}


	private:
		std::thread m_thread;


	};
}
		
