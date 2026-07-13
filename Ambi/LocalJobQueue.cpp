#include "LocalJobQueue.h"
#include <thread>

void LocalJobQueue::ProcessJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues)
{
	std::vector<std::function<void()>>& jobs = GetJobs();

	while (true)
	{
		std::function<void()> job;
		{
			std::lock_guard<std::mutex> lock(this->mtx);
			if (!jobs.empty())
			{
				job = std::move(jobs.front());
				jobs.erase(jobs.begin()); // Remove the job from the queue since its being executed
			}
		}
		// Leave the mutex here, we dont need to hold it and doing so will result in a dead lock in the 'stealJobs' function since it also needs to lock the mutex of the other thread's job queue
		if (!stealJobs(threadQueues, job))
		{
			break; // No more jobs to process and no jobs to steal
		}
		job(); // Execute the job outside of the lock to avoid blocking other threads
	}
}

bool LocalJobQueue::stealJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues, std::function<void()>& job)
{
	if (threadQueues.size() <= 1)
	{
		return false; // No other threads to steal from
	}

	for (int i = 0; i < threadQueues.size(); ++i)
	{
		if (threadQueues[i].get() == this)
		{
			continue; // Skip the current thread's queue
		}

		std::vector<std::function<void()>>& otherJobs = threadQueues[i]->GetJobs();
		{
			std::lock_guard<std::mutex> lock(this->mtx);
			if (!otherJobs.empty())
			{
				job = std::move(otherJobs.back());
				otherJobs.pop_back(); // Remove the job from the other thread's queue since its being stolen
				return true; // Successfully stole a job
			}
		}
	}
	return false; // Failed to steal a job
}