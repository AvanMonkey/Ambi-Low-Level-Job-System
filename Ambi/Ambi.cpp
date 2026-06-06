// Ambi.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include "JobQueue.h"

int main()
{
	bool running = true;

	const auto numberOfThreads = std::thread::hardware_concurrency();	// Let developer know how many threads the device has, so they can decide how many threads to use for their application.

	std::cout << "Number of threads on this device: " << numberOfThreads << "\n\n";

	while (running)
	{
		JobQueue jobQueue;	// Create a JobQueue instance to manage the global queue of jobs. This will be used to add jobs and distribute them to worker threads.

		const std::vector<std::function<void()>>& globalQueue = jobQueue.GetJobs();	// Get the current global queue of jobs. This allows worker threads to access the jobs they need to process, with a reference being returned so the user can see the current state of the queue.

		/*
		* Main loop of the application, Will be used to manage jobs using a thread pool, worker threads and job queues.
		*/
		break;
	}
	return 0;
}