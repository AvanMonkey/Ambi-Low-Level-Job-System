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

		threadsJobQueues[threadNum]->AddJob(jobs[i]); // Add the job to the local queue of the corresponding worker thread
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

	for (int i = 0; i < threadsInUse.size(); i++)
	{
		threadsJobQueues[i]->ProcessJobs(threadsJobQueues); // We use a lamda since we need to create a new thread each time
	}

	// Join the threads when done
	for (auto& thread : threadsInUse)
	{
		for (auto& threadQueue : threadsJobQueues)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}
	}
}