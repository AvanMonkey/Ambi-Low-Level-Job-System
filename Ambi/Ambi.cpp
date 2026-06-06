// Ambi.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
int main()
{
	bool running = true;

	const auto numberOfThreads = std::thread::hardware_concurrency();	// Let developer know how many threads the device has, so they can decide how many threads to use for their application.

	std::cout << "Number of threads on device: " << numberOfThreads << std::endl;

	while (running)
	{
		break;
	}
	return 0;
}