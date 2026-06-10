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
	 * @brief Process the jobs in the local queue.
	 *
	 * Process the jobs in the local queue. This will be used by each worker thread to process the jobs that are assigned to it, and later on, will steal jobs from other workers.
	 */
	void ProcessJobs();
};