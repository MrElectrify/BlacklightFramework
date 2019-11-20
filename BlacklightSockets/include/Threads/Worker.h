#ifndef BLACKLIGHT_THREADS_WORKER_H_
#define BLACKLIGHT_THREADS_WORKER_H_

/*
A worker thread implementation, similar to an io_context
4/18/19 20:31
*/

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>

namespace Blacklight
{
	namespace Threads
	{
		/*
		A simple worker with a std::function<void> queue
		*/
		class Worker
		{
		public:
			// default constructor, initialize members
			Worker();

			using Job_t = std::function<void()>;
			// adds a job to the worker's queue of jobs, lvalue reference, also notifies worker thread
			void QueueJob(const Job_t& job);
			// adds a job to the worker's queue of jobs, rvalue reference, also notifies worker thread
			void QueueJob(Job_t&& job);

			// runs the job queue and returns when finished, unless it expects more work
			void Run();

			// runs at most single job and return when finished
			void RunOne();

			// notifies the worker that it should expect more work
			void NotifyExpectWork();

			// notifies the worker that it should no longer expect more work
			void NotifyFinishedWork();

			// stop a worker from executing any future jobs
			void Stop();

			~Worker();
		private:
			std::atomic<bool> m_expectWork;
			std::condition_variable m_conVar;
			std::queue<Job_t> m_jobs;
			std::mutex m_jobMutex;
		};
	}
}

#endif