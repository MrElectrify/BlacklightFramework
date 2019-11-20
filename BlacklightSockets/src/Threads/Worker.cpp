#include <Threads/Worker.h>

#include <chrono>

using Blacklight::Threads::Worker;

Worker::Worker() : m_expectWork(false) {}

void Worker::QueueJob(const Job_t& job)
{
	// thread safety
	std::lock_guard<std::mutex> jobGuard(m_jobMutex);

	m_jobs.push(job);

	m_conVar.notify_one();
}

void Worker::QueueJob(Job_t&& job)
{
	// thread safety
	std::lock_guard<std::mutex> jobGuard(m_jobMutex);

	m_jobs.push(std::move(job));

	m_conVar.notify_one();
}

void Worker::Run()
{
	std::unique_lock<std::mutex> lock(m_jobMutex);

	// make sure we are justified in waiting
	if (m_jobs.size() == 0 &&
		m_expectWork == false)
		return;

	while (true)
	{
		// we need to break out if we want to stop expecting work
		m_conVar.wait(lock, [this] { return m_jobs.size() > 0 || m_expectWork == false; });
		if (m_jobs.size() > 0)
		{
			auto job = m_jobs.front();
			m_jobs.pop();
			// unlock so that the job can queue other jobs if necessary
			lock.unlock();
			job();
			lock.lock();
		}
		else
			break;
	}
}

void Worker::RunOne()
{
	std::unique_lock<std::mutex> lock(m_jobMutex);

	// make sure we are justified in waiting
	if (m_jobs.size() == 0 &&
		m_expectWork == false)
		return;

	m_conVar.wait(lock, [this] { return m_jobs.size() > 0 || m_expectWork == false; });
	if (m_jobs.size() > 0)
	{
		auto job = m_jobs.front();
		m_jobs.pop();
		// unlock so that the job can queue other jobs if necessary
		lock.unlock();
		job();
		lock.lock();
	}
	else
		return;
}

void Worker::NotifyExpectWork()
{
	m_expectWork = true;
}

void Worker::NotifyFinishedWork()
{
	m_expectWork = false;

	m_conVar.notify_all();
}

void Worker::Stop()
{
	NotifyFinishedWork();
}

Worker::~Worker()
{
	Stop();
}