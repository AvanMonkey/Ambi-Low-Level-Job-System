#pragma once
#include "JobQueue.h"
#include <mutex>
/**
* @class LocalJobQueue
* @brief Local Queue for Jobs. This class is responsible for managing the local queue of jobs that will be processed by each worker thread.
* 
* Holds the Jobs that need to be processed by each worker thread. The LocalJobQueue class provides methods to add jobs to the local queue and process the jobs in the local queue.
*/
class LocalJobQueue : public JobQueue
{
public:
	/**
	 * @brief Process the jobs in the local queue.
	 *
	 * Process the jobs in the local queue. This will be used by each worker thread to process the jobs that are assigned to it, and later on, will steal jobs from other workers.
	 *
	 * @param threadQueues The vector of local job queues for each worker thread. To be passed to the 'stealJobs' function to allow for job stealing between threads.
	 */
	void ProcessJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues);

	/**
	 * @brief Steal jobs from other worker threads.
	 *
	 * Steal jobs from other worker threads' local job queues. This is used to balance the load between threads.
	 *
	 * @param threadQueues The vector of local job queues for each worker thread.
	 * @param job The job to be stolen.
	 * @return True if a job was successfully stolen, false otherwise.
	 */
	bool stealJobs(std::vector<std::unique_ptr<LocalJobQueue>>& threadQueues, std::function<void()>& job);

private:
	std::mutex mtx;
};