#include "LocalJobQueue.h"
#include <thread>

void LocalJobQueue::ProcessJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues)
{
	std::vector<std::function<void()>>& jobs = GetJobs();

	while (true)
	{
		std::function<void()> job;
		if (!jobs.empty())
		{
			{
				std::lock_guard<std::mutex> lock(this->mtx);
				job = std::move(jobs.front());
				jobs.erase(jobs.begin()); // Remove the job from the queue since its being executed
			}
			job(); // Execute the job. Deal with it now so we dont accidentally overwrite it when we steal a job from another thread
		}

		if (jobs.empty() && stealJobs(threadQueues, job))
		{
			job(); // Job will be changed to the stolen job in the 'stealJobs' function, so we can execute it here
		}
		else if (jobs.empty() && !stealJobs(threadQueues, job))
		{
			break; // No more jobs to process and no jobs to steal
		}
	}
}

bool LocalJobQueue::stealJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues, std::function<void()>& job)
{
	if (threadQueues.size() < 2)
	{
		return false; // No other threads to steal from (Need atleast 3 present if we wanna steal jobs)
	}

	for (int i = 0; i < threadQueues.size(); ++i)
	{
		LocalJobQueue* currentQueue = threadQueues[i].get();

		if (currentQueue == this)
		{
			continue; // Skip the current thread's queue
		}

		// Try to pop a job from the other thread's queue) and steal it for this worker thread. We use a reference to the job so we can execute it in the current thread after stealing it
		if (currentQueue->tryPopJob(job))
		{
			return true; // Successfully stole a job
		}
	}
	return false; // Failed to steal a job
}

bool LocalJobQueue::tryPopJob(std::function<void()>& job)
{
	// We make sure to lock the mutex in the owner thread's queue to prevent data races.
	std::lock_guard<std::mutex> lock(this->mtx);
	if (!GetJobs().empty())
	{
		job = std::move(GetJobs().back());
		GetJobs().pop_back();
		return true;
	}
	return false;
}