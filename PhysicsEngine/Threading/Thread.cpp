
#include "Thread.h"


namespace Threading
{
	void Thread::Join()
	{
		if (m_thread.joinable())
			m_thread.join();
	}
	void Thread::Detach()
	{
		if (m_thread.joinable())
			m_thread.detach();
	}
}