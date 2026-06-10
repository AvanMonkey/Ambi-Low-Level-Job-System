// This is the file for testing the Ambi library 

#include <iostream>
#include <thread>
#include <random>
#include "../Ambi/WorkerThreads.h"
#include "../Ambi/JobQueue.h"
#include "../Ambi/LocalJobQueue.h"

void TestJob()
{
	std::cout << "Hello from the Job!\n\n";
}

void TestJob2()
{
	std::cout << "I'm another Job :)\n\n";
}

int main()
{
	JobQueue globalJobQueue;
	WorkerThreads workerThreads;
	// Get the vectors by reference since there should only be one instance of the worker threads and their job queues. We also need to access them throughout the whole program
	std::vector<std::thread>& threads = workerThreads.getThreads();
	std::vector<LocalJobQueue>& threadsJobQueues = workerThreads.getThreadsJobQueues();

	// Testing a bunch of random number of jobs
	std::random_device rd;
	std::mt19937 gen(rd());
	int min = 6;
	int max = 100000;
	std::uniform_int_distribution<int> dist(min, max);
	int numThreads = 0;

	if (std::thread::hardware_concurrency() != 0)
	{
		// See the number of threads available on the device's thread pool (-1 for main thread).
		numThreads = std::thread::hardware_concurrency() - 1;
	}
	else
	{
		std::cout << "Not enough Threads to use this class";
		throw;
	}

	// Generate the jobs that need doing and add them to a global job queue. Max number is random for the sake of testing different loads
	for (int i = 0; i < dist(gen); i++)
	{
		if (i % 3 == 0) // 3 Specifically cos if i do 2 theyll probably just take turns printing
		{
			globalJobQueue.AddJob(TestJob);
		}
		else 
		{
			globalJobQueue.AddJob(TestJob2);
		}
	}

	std::cout << "Number of threads on device: " << numThreads << "\n\n";
	std::cout << "Number of Jobs: " << globalJobQueue.GetJobs().size() << "\n\n";

	workerThreads.distributeJobsToLocalQueues(globalJobQueue); // Distribute the jobs to the local queues of the worker threads
	workerThreads.executeJobs(); // Execute the jobs on the worker threads

	std::cout << "All jobs processed.\n";
	return 0;
}