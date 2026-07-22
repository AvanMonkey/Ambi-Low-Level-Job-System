#pragma once
#include <vector>
#include <thread>
#include "LocalJobQueue.h"
#include <iostream>
#include <mutex>

/**
* @class WorkerThreads
* @brief Holds the number of threads available on the device's thread pool.
*
* Initialises a vector containing the amount of worker threads (-1 for the main thread) available on the device.
* This can then be used to assign these threads jobs and use them for other functions
*/
class WorkerThreads
{
public:
	/**
	 * @brief Constructor for 'WorkerThreads' class. Initialises a vector containing the available worker threads available for use, and their job queues
	 *
	 * Threads can be used to execute jobs in parallel, for more efficient processing.
	*/
	WorkerThreads() {
		int numThreads = 0;

		if (std::thread::hardware_concurrency() != 0)
		{
			// See the number of threads available on the device's thread pool (-1 for main thread).
			numThreads = std::thread::hardware_concurrency() - 1;
		}
		else
		{
			std::cout << "Not enough Threads to use this class";
			throw _EXCEPTION_;
		}

		for (int i = 0; i < numThreads; i++) // Leave one thread for the main thread to run on
		{
			threads.emplace_back();
			threadsJobQueues.emplace_back(std::make_unique<LocalJobQueue>());
		}
	}

	/**
	 * @brief Return the vector of worker threads available for use
	 *
	 * 'threads' vector is private, so a getter is needed to access it. This allows the user to see the available worker threads and assign jobs to them as needed
	*/
	std::vector<std::thread>& getThreads() { return threads; };

	/**
	 * @brief Return the vector of jobs. 
	 *
	 * For accessing the job queues of threads. Their index corresponds to the worker thread in the 'threads' vector.
	*/
	std::vector<std::unique_ptr<LocalJobQueue>>& getThreadsJobQueues() { return threadsJobQueues; };

	/**
	* @brief Add the global jobs to the local queues of the worker threads.
	* 
	*  Self Explanatory
	* 
	* @param globalJobQueue The global job queue that contains the jobs to be distributed to the worker threads. Passed by reference since we need to access the singular job queue throughout the whole program
	*/
	void distributeJobsToLocalQueues(JobQueue& globalJobQueue);

	/**
	 * @brief Execute the jobs on the worker threads.
	 *
	 * This will execute the jobs on the worker threads, Creating a new thread for each worker thread and execute the jobs in their local queues. It will then join the threads when done.
	*/
	void executeJobs();

	/**
	* @brief Allow worker threads to steal jobs from each other.
	*
	* This function enables work stealing between worker threads, improving overall processing efficiency.
	*/
	void stealJobsFromOtherThreads(); 

	std::mutex mutex; // Mutex for synchronizing access to shared resources, ensuring thread safety during job distribution and execution
private:

	/**
	 * @brief Vector of the worker threads available for use.
	 *  Initialised in the constructor
	*/
	std::vector<std::thread> threads;

	/**
	 * @brief Vector containing each worker thread's local job queue.
	 * Initilaised in the constructor. This allows each worker thread to have its own queue of jobs to process, and later on, will allow for work stealing between threads.
	*/
	std::vector<std::unique_ptr<LocalJobQueue>> threadsJobQueues;
};