#pragma once
#include "JobQueue.h"

/**
* @class LocalJobQueue
* @brief Local Queue for Jobs. This class is responsible for managing the local queue of jobs that will be processed by each worker thread.
* 
* Holds the Jobs that need to be processed by each worker thread. The LocalJobQueue class provides methods to add jobs to the local queue and process the jobs in the local queue.
*/
class LocalJobQueue : public JobQueue
{
public:
	LocalJobQueue() = default;

	/**
	 * @brief Add a job to the local queue.
	 *
	 * Override Base method to add a job to the local queue for the designated worker thread.
	 * @param job The Job to be added to the local queue. This is a std::function that represents any callable type, allowing for flexibility in the types of jobs that can be added to the queue
	 */
	void AddJob(std::function<void()> job) override;

	/**
	 * @brief Process the jobs in the local queue.
	 *
	 * Override Base method to process the jobs in the local queue. This will be used by each worker thread to process the jobs that are assigned to it, and later on, will steal jobs from other workers.
	 */
	void ProcessJobs() override;
};