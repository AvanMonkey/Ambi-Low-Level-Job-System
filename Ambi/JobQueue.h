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

	/**
	 * @brief Jobs will vary in their bodies, so we use std::function to allow for any callable type to be added as a job.
	 *
	 * Add Jobs to the global queue. This queue will later be distributed across each available worker thread. This will obviously depend on the implementation of the thread pool and worker threads,
	 * but the idea is that each worker thread will pull jobs from this global queue and execute them.
	 * @param job The Job to be added to the global queue. This is a std::function that represents any callable type, allowing for flexibility in the types of jobs that can be added to the queue
	*/
	void virtual AddJob(std::function<void()> job);

	/**
	 * @brief A method to retrieve the current queue of jobs. This allows worker threads to access the jobs they need to process, with a reference being returned so the user can see the current state of the queue.
	 *
	 * Return the current global queue of jobs to be used in the program. This allows worker threads to access the jobs they need to process, with a reference being returned so the user can see the current state of the queue.
	 * The developer cannot edit the queue directly, but they can see the current state of the queue and how many jobs are currently in it. This is useful for debugging and monitoring the job queue.
	 * @return The Global Job Queue
	*/
	const std::vector<std::function<void()>>& GetJobs() { return jobQueue; };

	/**
	 * @brief Distribute Jobs to worker threads.
	 *
	 * Jobs will be distributed depending on the amount of worker threads are requested to be used by the developer. Maximum will be the amount of threads the device has.
	*/
	void virtual ProcessJobs();
private:

	/**
	 * @brief The Vector that holds the global queue of jobs.
	 *
	 * Holds all global jobs that need to be processed by the worker threads.
	*/
	std::vector<std::function<void()>> jobQueue;
};