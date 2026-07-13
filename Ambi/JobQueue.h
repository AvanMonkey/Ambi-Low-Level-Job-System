#pragma once
#include <functional>
/**
* @class JobQueue
* @brief Global Queue for Jobs. This class is responsible for managing the global queue of jobs that will be distributed to worker threads.
* 
* Contains the Jobs that need to be processed by the worker threads. The JobQueue class provides methods to add jobs to the queue and retrieve the current queue of jobs.
*/
class JobQueue
{
public:
	JobQueue() = default;

	JobQueue(const JobQueue&) = delete;
	JobQueue& operator=(const JobQueue&) = delete;
	JobQueue(JobQueue&&) = delete;
	JobQueue& operator=(JobQueue&&) = delete;

	/**
	 * @brief Jobs will vary in their bodies, so we use std::function to allow for any callable type to be added as a job.
	 *
	 * Add Jobs to the queue. This queue will later be distributed across each available worker thread. This will obviously depend on the implementation of the thread pool and worker threads,
	 * but the idea is that each worker thread will pull jobs from this queue and execute them.
	 * @param job The Job to be added to the queue. This is a std::function that represents any callable type, allowing for flexibility in the types of jobs that can be added to the queue
	*/
	void AddJob(std::function<void()> job);

	/**
	 * @brief A method to retrieve the current queue of jobs. This allows worker threads to access the jobs they need to process, with a reference being returned so the user can see the current state of the queue.
	 *
	 * Return the current global queue of jobs to be used in the program. This allows worker threads to access the jobs they need to process, with a reference being returned so the user can see the current state of the queue.
	 * The developer cannot edit the queue directly, but they can see the current state of the queue and how many jobs are currently in it. This is useful for debugging and monitoring the job queue.
	 * @return The Global Job Queue
	*/
	std::vector<std::function<void()>>& GetJobs() { return jobQueue; };
private:

	/**
	 * @brief The Vector that holds the queue of jobs.
	 *
	 * Holds all jobs that need to be processed by the worker threads.
	*/
	std::vector<std::function<void()>> jobQueue;
};