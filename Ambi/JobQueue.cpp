#include "JobQueue.h"

void JobQueue::AddJob(std::function<void()> job)
{
	jobQueue.push_back(job);	// Add the job to the queue.
}

void JobQueue::ProcessJobs()
{
	// Process the jobs in the queue.
}