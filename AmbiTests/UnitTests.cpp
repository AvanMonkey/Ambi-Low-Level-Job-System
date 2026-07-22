// test_job_stealing.cpp
//
// Boost.Test suite for JobQueue / LocalJobQueue / WorkerThreads.
//
// Tests the 'Ambi' library's job queueing and work-stealing behavior, including concurrency.

#define BOOST_TEST_MODULE JobStealingTests
#include <boost/test/included/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include "../Ambi/WorkerThreads.h"
#include "../Ambi/JobQueue.h"
#include "../Ambi/LocalJobQueue.h"

// -----------------------------------------------------------------------
//                JobQueue: single-threaded behavior
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(JobQueueBasicTests)


// -------------------------------------------------------------------
// Check that the queue is empty upon construction, and that GetJobs() returns an empty vector. This is important to ensure that the queue starts in a known state
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(StartsEmpty)
{
    JobQueue queue;
    BOOST_TEST(queue.GetJobs().empty());
}


// -------------------------------------------------------------------
// Check that jobs are actually added to a queue and that the size of the queue increases accordingly.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(AddJob_IncreasesSize)
{
    JobQueue queue;
    queue.AddJobs([] {});
    BOOST_TEST(queue.GetJobs().size() == 1u);

    queue.AddJobs([] {});
    queue.AddJobs([] {});
    BOOST_TEST(queue.GetJobs().size() == 3u);
}


// -------------------------------------------------------------------
// Check that Added jobs are callable and can be invoked later.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(AddJob_StoresCallableThatCanBeInvoked)
{
    JobQueue queue;
    int callCount = 0;
    queue.AddJobs([&callCount] { ++callCount; });

    // GetJobs() returns a mutable reference per the header, so the
    // caller can invoke stored jobs directly.
    for (auto& job : queue.GetJobs())
    {
        job();
    }
    BOOST_TEST(callCount == 1);
}


// -------------------------------------------------------------------
// Check that Jobs added are in the correct order (FIFO) when retrieved from the queue. This is important for predictable execution order, especially if jobs have dependencies or side effects.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(AddJob_PreservesInsertionOrder)
{
    JobQueue queue;
    std::vector<int> order;
    for (int i = 0; i < 5; ++i)
    {
        queue.AddJobs([&order, i] { order.push_back(i); });
    }
    for (auto& job : queue.GetJobs())
    {
        job();
    }
    std::vector<int> expected{ 0, 1, 2, 3, 4 };
    BOOST_TEST(order == expected, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------
//                        JobQueue: Concurrency
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(JobQueueConcurrencyTests)

// -------------------------------------------------------------------
// Check that jobs added concurrently are eventually present in the queue
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ConcurrentAddJob_AllJobsEventuallyPresent)
{
    JobQueue queue;
    constexpr int numThreads = 4;
    constexpr int jobsPerThread = 500;

    std::vector<std::thread> producers;
    producers.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t)
    {
        producers.emplace_back([&queue, jobsPerThread] {
            for (int i = 0; i < jobsPerThread; ++i)
            {
                queue.AddJobs([] {});
            }
            });
    }
    for (auto& t : producers) t.join();

    // If AddJob() is unsynchronized, this count can legitimately come
    // out wrong (or the run can crash before reaching here). Run under
    // TSan/Helgrind to confirm whether that's actually happening.
    BOOST_TEST(queue.GetJobs().size() == static_cast<size_t>(numThreads * jobsPerThread));
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------
//                  LocalJobQueue: Stealing Behavior
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(LocalJobQueueTests)


// -------------------------------------------------------------------
// Check that jobs aren't stolen if there are fewer than two queues, even if the queue has work queued. This is because having 2 threads would mean there's only one worker thread, since the 2nd thread is the main thread, and stealing from itself is not allowed.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(StealJobs_ReturnsFalse_WhenFewerThanTwoQueues)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    queues.push_back(std::make_unique<LocalJobQueue>());
    queues[0]->AddJobs([] {}); // even with work queued, size() < 2 short-circuits

    std::function<void()> stolen;
    BOOST_TEST(!queues[0]->stealJobs(queues, stolen));
}


// -------------------------------------------------------------------
// Check that when every queue is empty, stealJobs() returns false and does not modify the stolen job reference.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(StealJobs_ReturnsFalse_WhenAllOtherQueuesEmpty)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < 3; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    std::function<void()> stolen;
    BOOST_TEST(!queues[0]->stealJobs(queues, stolen));
}


// -------------------------------------------------------------------
// Check that stealing jobs pops from the back of the victim's queue
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(StealJobs_ReturnsTrue_AndPopsVictimsBack)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < 3; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    std::vector<int> order;
    queues[1]->AddJobs([&order] { order.push_back(1); }); // pushed first
    queues[1]->AddJobs([&order] { order.push_back(2); }); // pushed last -> should be stolen first (back)

    std::function<void()> stolen;
    bool result = queues[0]->stealJobs(queues, stolen);

    BOOST_TEST(result);
    BOOST_REQUIRE(static_cast<bool>(stolen));
    stolen();
    BOOST_TEST(order.size() == 1u);
    BOOST_TEST(order[0] == 2); // confirms back(), not front(), was stolen

    // Victim should now have exactly one job left.
    BOOST_TEST(queues[1]->GetJobs().size() == 1u);
}


// -------------------------------------------------------------------
// Check that the stealer's own queue is skipped, since why would a thread steal from itself?
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(StealJobs_SkipsSelf_EvenWhenSelfHasWork)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < 2; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    // Only the stealer's own queue has work; nothing to take from anyone else.
    queues[0]->AddJobs([] {});

    std::function<void()> stolen;
    BOOST_TEST(!queues[0]->stealJobs(queues, stolen));
    BOOST_TEST(queues[0]->GetJobs().size() == 1u); // untouched
}


// -------------------------------------------------------------------
// Check that the queue with the earliest index is raided first, even if a later queue has more work. This is important for fairness and to avoid starvation of lower-indexed queues.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(StealJobs_PrefersEarliestIndexWithWork)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < 4; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    bool queue2Ran = false, queue3Ran = false;
    queues[2]->AddJobs([&queue2Ran] { queue2Ran = true; });
    queues[3]->AddJobs([&queue3Ran] { queue3Ran = true; });

    // queues[0] steals: scan order is 1, 2, 3 (skipping self at 0).
    // queue 1 is empty, so queue 2 should be the one raided.
    std::function<void()> stolen;
    BOOST_REQUIRE(queues[0]->stealJobs(queues, stolen));
    stolen();

    BOOST_TEST(queue2Ran);
    BOOST_TEST(!queue3Ran);
}


// -------------------------------------------------------------------
// Check that each thread executes it's own queue before attempting to steal from others, and that the order of execution is FIFO for its own queue.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ProcessJobs_RunsOwnQueueInFifoOrder_ThenSteals)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < 2; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    std::vector<int> order;
    queues[0]->AddJobs([&order] { order.push_back(1); });
    queues[0]->AddJobs([&order] { order.push_back(2); });
    queues[1]->AddJobs([&order] { order.push_back(3); }); // to be stolen once queue 0 is drained

    queues[0]->ProcessJobs(queues);

    std::vector<int> expected{ 1, 2, 3 };
    BOOST_TEST(order == expected, boost::test_tools::per_element());
    BOOST_TEST(queues[1]->GetJobs().empty());
}


// -------------------------------------------------------------------
// Check that no data races occur when one thread is processing its own queue while another is attempting to steal from it.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ConcurrentStealAndProcess_NoDataRace)
{
    constexpr int numQueues = 4;
    constexpr int numJobs = 500;

    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < numQueues; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    std::atomic<int> executionCount{ 0 };
    for (int i = 0; i < numJobs; ++i)
    {
        queues[0]->AddJobs([&executionCount] { ++executionCount; });
    }

    // Queue 0 processes (and thus pops) its own front() while queues
    // 1..N concurrently try to steal from its back() -- the exact
    // unsynchronized pattern the wrong-mutex bug allows.
    std::vector<std::thread> workers;
    workers.emplace_back([&queues] { queues[0]->ProcessJobs(queues); });
    for (int i = 1; i < numQueues; ++i)
    {
        workers.emplace_back([&queues, i] {
            std::function<void()> job;
            while (queues[i]->stealJobs(queues, job))
            {
                if (job) job();
            }
            });
    }
    for (auto& t : workers) t.join();

    // Every job should run exactly once. Under the current locking bug
    // this can flake (miscount, or a sanitizer-flagged race) rather
    // than reliably hold.
    BOOST_TEST(executionCount.load() == numJobs);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------
//                            WorkerThreads
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(WorkerThreadsTests)


// -------------------------------------------------------------------
// Check that the amount of queues created matches the number of threads available on the device's thread pool (-1 for main thread). If there are not enough threads, skip the test.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Construction_CreatesQueuesMatchingThreadCount)
{
    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads to construct WorkerThreads safely.");
        return;
    }

    WorkerThreads pool;

    unsigned int expected = std::thread::hardware_concurrency() >= 2
        ? std::thread::hardware_concurrency() - 1
        : 0;

    BOOST_TEST(pool.getThreads().size() == expected);
    BOOST_TEST(pool.getThreadsJobQueues().size() == expected);
}

// -------------------------------------------------------------------
// Check that local queues are constructed and that distributeJobsToLocalQueues() distributes jobs in a round-robin fashion, and clears the global queue.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(DistributeJobsToLocalQueues_RoundRobinsAndClearsGlobalQueue)
{
    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads.");
        return;
    }

    WorkerThreads pool;
    JobQueue globalQueue;

    const size_t numLocalQueues = pool.getThreadsJobQueues().size();
    if (numLocalQueues == 0)
    {
        BOOST_TEST_MESSAGE("Skipping: no local queues constructed on this machine.");
        return;
    }

    const int totalJobs = static_cast<int>(numLocalQueues) * 3; // guarantee at least one full round
    for (int i = 0; i < totalJobs; ++i)
    {
        globalQueue.AddJobs([] {});
    }

    pool.distributeJobsToLocalQueues(globalQueue);

    // Global queue is drained.
    BOOST_TEST(globalQueue.GetJobs().empty());

    // Every local queue should have gotten an equal share (round-robin,
    // evenly divisible in this setup).
    size_t distributed = 0;
    for (auto& localQueue : pool.getThreadsJobQueues())
    {
        BOOST_TEST(localQueue->GetJobs().size() == static_cast<size_t>(totalJobs) / numLocalQueues);
        distributed += localQueue->GetJobs().size();
    }
    BOOST_TEST(distributed == static_cast<size_t>(totalJobs));
}


// -------------------------------------------------------------------
// Check that executeJobs() is correctly distributing jobs to the local queues of the worker threads, and emptying the global queue. This is a no-op test, since the global queue is empty, but it should not crash or touch any local queues.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(DistributeJobsToLocalQueues_NoOp_WhenGlobalQueueEmpty)
{
    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads.");
        return;
    }

    WorkerThreads pool;
    JobQueue globalQueue;

    pool.distributeJobsToLocalQueues(globalQueue); // should return early, touching nothing

    for (auto& localQueue : pool.getThreadsJobQueues())
    {
        BOOST_TEST(localQueue->GetJobs().empty());
    }
}

// -------------------------------------------------------------------
// Check that executeJobs() executes all jobs in the local queues of the worker threads
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ExecuteJobs_RunsEveryDistributedJob)
{
    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads.");
        return;
    }

    WorkerThreads pool;
    JobQueue globalQueue;

    constexpr int totalJobs = 50;
    std::atomic<int> ran{ 0 };
    for (int i = 0; i < totalJobs; ++i)
    {
        globalQueue.AddJobs([&ran] { ++ran; });
    }

    pool.distributeJobsToLocalQueues(globalQueue);
    pool.executeJobs();

    BOOST_TEST(ran.load() == totalJobs);
}

// -------------------------------------------------------------------
// Check that executeJobs() actually runs on worker threads, not just on the calling thread.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ExecuteJobs_RunsOnWorkerThreads_NotCallingThread)
{
    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads.");
        return;
    }

    WorkerThreads pool;
    JobQueue globalQueue;

    const auto callingThreadId = std::this_thread::get_id();
    std::atomic<bool> ranOnDifferentThread{ false };

    constexpr int totalJobs = 10;
    for (int i = 0; i < totalJobs; ++i)
    {
        globalQueue.AddJobs([&ranOnDifferentThread, callingThreadId] {
            if (std::this_thread::get_id() != callingThreadId)
            {
                ranOnDifferentThread = true;
            }
            });
    }

    pool.distributeJobsToLocalQueues(globalQueue);
    pool.executeJobs();

    BOOST_TEST(ranOnDifferentThread.load());
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------
//                          Complex Scenarios
//                (These were written by myself, NOT AI)
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(ComplexTests)

// -------------------------------------------------------------------
// Check if complex scenarios with multiple threads execute across multiple threads.
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ExecuteJobs_ComplexScenario_SpeedComparison)
{
    WorkerThreads pool;
    JobQueue globalJobQueue;

    constexpr int totalJobs = 1000000;

    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads.");
        return;
    }

    std::vector<int> sequentialNumbers(totalJobs);
	std::chrono::steady_clock::time_point startSequential, endSequential;

	startSequential = std::chrono::steady_clock::now();

    for (int i = 0; i < totalJobs; ++i) // Large Sequential execution to compare against parallel latency
    {
        int sum = 0;
        for (int i = 0; i < 10000; ++i) sum += i; // simulate real work
        sequentialNumbers[i] = sum;
    }

	endSequential = std::chrono::steady_clock::now();

	std::chrono::steady_clock::time_point startParallel, endParallel;

    std::atomic<size_t> nextIndex{ 0 };
    std::vector<int> parallelNumbers(totalJobs); // pre-sized, no reallocation ever

    std::function<void()> job = [&parallelNumbers, &nextIndex]() {
        size_t idx = nextIndex.fetch_add(1, std::memory_order_relaxed);
        int sum = 0;
        for (int i = 0; i < 10000; ++i) sum += i; // simulate real work
        parallelNumbers[idx] = sum;
        };

    startParallel = std::chrono::steady_clock::now();

	for (int i = 0; i < totalJobs; ++i)
	{
		globalJobQueue.AddJobs(job);
    };

    pool.distributeJobsToLocalQueues(globalJobQueue);
    pool.executeJobs();

	endParallel = std::chrono::steady_clock::now();

    long long sequentialTime = std::chrono::duration_cast<std::chrono::milliseconds>(endSequential - startSequential).count();
	long long parallelTime = std::chrono::duration_cast<std::chrono::milliseconds>(endParallel - startParallel).count();
	
    BOOST_TEST(parallelNumbers.size() == totalJobs);
	BOOST_TEST(sequentialNumbers.size() == totalJobs);
	printf("Sequential execution time: %lld ms\n", sequentialTime);
	printf("Parallel execution time: %lld ms\n", parallelTime);
	// The parallel execution time should be less than the sequential execution time
	BOOST_TEST(parallelTime < sequentialTime);
}

// -------------------------------------------------------------------
// Check if a thread will steal jobs from another thread that is specifically backed up with a large number of jobs
// -------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ExecuteJobs_ComplexScenario_LoadBalancing)
{
    WorkerThreads pool;
    JobQueue globalJobQueue;

    constexpr int totalJobs = 1000000;

    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads.");
        return;
    }

	std::atomic<size_t> nextIndex{ 0 }; // start at 1 since we will manually assign the first job to a thread to simulate a thread being backed up
    std::vector<int> parallelNumbers(totalJobs); // pre-sized, no reallocation ever
    std::vector<std::thread::id> executedBy(totalJobs);
    std::thread::id bigJobThreadID;

    // Singular Large Job to be used in 1 thread to simulate a thread being backed up, and the other threads will steal from it to balance the load
    std::function<void()> job = [&parallelNumbers, &nextIndex, &executedBy, &bigJobThreadID]() {
        size_t idx = nextIndex.fetch_add(1, std::memory_order_relaxed);
        int sum = 0;
        for (int i = 0; i < 1000000; ++i) sum += i;
        parallelNumbers[idx] = sum;
		bigJobThreadID = std::this_thread::get_id(); // store the thread ID of the thread that is executing this job. Should be thread 0, since it is the first thread in the array and will be distributed to it
        };

    globalJobQueue.AddJobs(job);

    job = [&parallelNumbers, &nextIndex, &executedBy]() {
        size_t idx = nextIndex.fetch_add(1, std::memory_order_relaxed);
        int sum = 0;
        for (int i = 0; i < 10000; ++i) sum += i; // simulate real work
        parallelNumbers[idx] = sum;
		executedBy[idx] = std::this_thread::get_id(); // store the thread ID of a thread that is executing this job. Should be different from the thread that is executing the big job, since it will be stolen from it
        };


    for (int i = 0; i < totalJobs - 1; ++i)
    {
        globalJobQueue.AddJobs(job);
    };

    pool.distributeJobsToLocalQueues(globalJobQueue);
    pool.executeJobs();

    BOOST_TEST(bigJobThreadID != std::thread::id{}); // sanity check :(

	size_t numThreads = pool.getThreads().size();
	// Chunks are used since its how many jobs would have been assigned to thread 0, where the big job was assigned to.
    size_t chunkSize = totalJobs / numThreads; // assumes contiguous chunk distribution because we're using a vector to hold them
	int stolenFromChunk = 0;

    // We need to check how many jobs were stolen from the thread chunk.
    for (size_t idx = 1; idx < chunkSize; ++idx)
    {
        if (executedBy[idx] != bigJobThreadID)
        {
            ++stolenFromChunk;
        }
    }

    int notStolen = static_cast<int>(chunkSize - 1) - stolenFromChunk; // how many chunk-jobs thread 0 got to itself

    printf("Jobs originally in backed-up thread's chunk (Not including the big job): %zu\n", chunkSize - 1);
    printf("Jobs stolen from that chunk: %d\n", stolenFromChunk);
    printf("Jobs not stolen from that chunk: %d\n", notStolen);
    BOOST_TEST(stolenFromChunk > 0); // at least some stealing happened
}
BOOST_AUTO_TEST_SUITE_END()