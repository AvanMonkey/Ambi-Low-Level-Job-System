// This is the file for testing the Ambi library 

#include <iostream>
#include <thread>
#include "../Ambi/WorkerThreads.h"
#include "../Ambi/JobQueue.cpp"
#include "../Ambi/LocalJobQueue.cpp"
#include <random>
void TestJob()
{
	std::cout << "Hello from the Job!\n";
}

int main()
{
	JobQueue globalJobQueue;
	WorkerThreads workerThreads;

	std::vector<std::thread>& threads = workerThreads.getThreads();
	std::vector<LocalJobQueue> threadsJobQueues = workerThreads.getThreadsJobQueues();

	// Testing a bunch of random number of jobs
	std::random_device rd;
	std::mt19937 gen(rd()); // Mersenne Twister engine

	std::uniform_int_distribution<int> dist(6, 1000000000);

	// See the number of threads available on the device's thread pool.
	int numThreads = std::thread::hardware_concurrency();
    std::cout << "Number of threads on device: " << numThreads << "\n\n";

	// The jobs that need doing will be added to a global job queue. Max number is random for the sake of testing different job rquirements
	for (int i = 0; i < dist(gen); i++)
	{
		globalJobQueue.AddJob(TestJob);
	}

	std::cout << "Number of Jobs: " << globalJobQueue.GetJobs().size() << "\n\n"; // See the number of jobs in the global queue.

	// Add the global jobs to the local queues of the worker threads. This is a simple distribution method, and can be improved with more complex algorithms for better load balancing and efficiency.
	for (int i = 0; i < globalJobQueue.GetJobs().size(); i ++)
	{
		int threadNum = i % threads.size(); // Mod it so it loops back to the first thread if there are more jobs than threads

		threadsJobQueues[threadNum].AddJob(globalJobQueue.GetJobs()[i]); // Add the job to the local queue of the corresponding worker thread
	}

	// Each thread executes its designated local queue 
	for (int i = 0; i < threads.size(); i++)
	{
		threads[i] = std::thread{ &LocalJobQueue::ProcessJobs, &threadsJobQueues[i] }; // We use a lamda since we need to create a new thread each time
	}

	// Join the threads when done
	for (auto& thread : threads)
	{
		if (thread.joinable())
		{
			thread.join(); 
		}
	}

	std::cout << "All jobs processed.\n";
	return 0;
}