#ifndef BLACKLIGHT_THREADS_EXECUTEGUARD_H_
#define BLACKLIGHT_THREADS_EXECUTEGUARD_H_

/*
Execute guard for a worker thread
4/18/19 20:56
*/

#include <Threads/Worker.h>

namespace Blacklight
{
	namespace Threads
	{
		/*
		A guard for an executor that notifies it that it should expect work
		*/
		template<typename Executor>
		class ExecuteGuard
		{
		public:
			// notifies the executor that it should expect more work
			ExecuteGuard(Executor& executor) : m_executor(executor) { m_executor.NotifyExpectWork(); }
			// notifies the executor that it should not expect more work
			~ExecuteGuard() { m_executor.NotifyFinishedWork(); }
		private:
			Executor& m_executor;
		};
	}
}

#endif