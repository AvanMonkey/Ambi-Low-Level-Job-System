// This is the file for testing the Ambi library 

#include <iostream>
#include <thread>

int main()
{
    std::cout << std::thread::hardware_concurrency();
}