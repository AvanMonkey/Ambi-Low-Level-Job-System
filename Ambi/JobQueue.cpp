#include "JobQueue.h"

void JobQueue::AddJobs(std::function<void()> job)
{
	std::lock_guard<std::mutex> lock(mtx);
	jobQueue.push_back(job);	// Add the job to the queue.
}
