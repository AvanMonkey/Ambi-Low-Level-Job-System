#pragma once
#include <vector>
#include <thread>
#include "LocalJobQueue.h"

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
		int numThreads = std::thread::hardware_concurrency();
		for (int i = 0; i < numThreads - 1; i++) // Leave one thread for the main thread to run on
		{
			threads.emplace_back();
			threadsJobQueues.emplace_back();
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
	std::vector<LocalJobQueue>& getThreadsJobQueues() { return threadsJobQueues; };
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
	std::vector<LocalJobQueue> threadsJobQueues;
};