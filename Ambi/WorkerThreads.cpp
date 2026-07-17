#include "WorkerThreads.h"

void WorkerThreads::distributeJobsToLocalQueues(JobQueue& globalJobQueue) {

	auto& jobs = globalJobQueue.GetJobs();

	if (jobs.empty())
	{
		return; // No jobs
	}

	for (int i = 0; i < jobs.size(); i++)
	{
		int threadNum = i % threads.size(); // Mod it so it loops back to the first thread if there are more jobs than threads

		threadsJobQueues[threadNum]->AddJobs(jobs[i]); // Add the job to the local queue of the corresponding worker thread
	}
	jobs.clear(); // Clear the global job queue after distributing the jobs to the local queues of the worker threads
}

void WorkerThreads::executeJobs() 
{
	std::vector<std::thread>& threadsInUse = getThreads();

	if (threadsJobQueues.empty())
	{
		return; // No jobs
	}

	// Trigger Jobs threads
	for (size_t i = 0; i < threadsInUse.size(); i++)
	{
		threadsInUse[i] = std::thread(&LocalJobQueue::ProcessJobs, threadsJobQueues[i].get(), std::ref(threadsJobQueues));
	}

	// Join the threads when done
	for (auto& thread : threadsInUse)
	{ 
		if (thread.joinable())
		{
			thread.join(); // Join the threads when done
		}
	}
}