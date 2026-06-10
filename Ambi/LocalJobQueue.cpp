#include "LocalJobQueue.h"
#include <thread>


void LocalJobQueue::ProcessJobs()
{
	std::vector<std::function<void()>> jobs = GetJobs();
	for (auto& job : jobs)
	{
		job();
	}
}