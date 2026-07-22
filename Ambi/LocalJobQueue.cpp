#include "LocalJobQueue.h"
#include <thread>

void LocalJobQueue::ProcessJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues)
{
	while (true)
	{
		std::function<void()> job;

		// Execute the jobs in this Local Job Queue
		if (tryEraseJob(job))
		{
			job(); // Deal with it now so we dont accidentally overwrite it when we steal a job from another thread
			continue;
		}

		bool isJobQueueEmpty;
		{
			std::lock_guard<std::mutex> lock(this->mtx);
			isJobQueueEmpty = GetJobs().empty();
		}

		if (!isJobQueueEmpty)
		{
			continue;
		}

		// Try stealing if conditions are met
		if (stealJobs(threadQueues, job))
		{
			job(); // Job will be changed to the stolen job in the 'stealJobs' function, so we can execute it here
		}
		else
		{
			break; // No more jobs to process and no jobs to steal
		}
	}
}

bool LocalJobQueue::stealJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues, std::function<void()>& job)
{
	// Need atleast 2 worker threads present if we wanna steal jobs, since 1 worker thread obviously cant steal from itself. The main thread is not a worker thread so isn't taken into account here.
	if (threadQueues.size() < 2)
	{
		return false; // No other threads to steal from
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


bool LocalJobQueue::tryEraseJob(std::function<void()>& job)
{
	std::lock_guard<std::mutex> lock(this->mtx);
	if (!GetJobs().empty())
	{
		job = std::move(GetJobs().front()); // (FIFO)
		GetJobs().pop_front(); // Remove the job from the Job queue since its being executed
		return true;
	}
	return false;
}

bool LocalJobQueue::tryPopJob(std::function<void()>& job)
{
	// We make sure to lock the mutex in the owner thread's queue to prevent data races.
	std::lock_guard<std::mutex> lock(this->mtx);
	if (!GetJobs().empty())
	{
		job = std::move(GetJobs().back()); // (LIFO)
		GetJobs().pop_back(); // Remove the job from the back of the Job queue since its being stolen
		return true;
	}
	return false;
}