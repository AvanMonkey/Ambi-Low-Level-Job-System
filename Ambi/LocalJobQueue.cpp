#include "LocalJobQueue.h"
#include <thread>


void LocalJobQueue::ProcessJobs()
{
	std::vector<std::function<void()>>& jobs = GetJobs();

	if (jobs.empty())
	{
		return; // No jobs
	}

	for (auto& job : jobs)
	{
		job();
	}

	jobs.clear(); // Clear the local job queue after processing the jobs
}