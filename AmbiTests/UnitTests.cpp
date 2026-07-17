// test_job_stealing.cpp
//
// Boost.Test suite for JobQueue / LocalJobQueue / WorkerThreads.
//
// Written against the real .cpp implementations. Two known bugs are
// deliberately encoded as tests that are EXPECTED TO FAIL right now:
//
//   1. LocalJobQueue::stealJobs() locks `this->mtx` (the thief's own
//      mutex) instead of the victim's mutex, so concurrent steals from
//      the same donor (or a donor popping its own front() at the same
//      time) are unsynchronized accesses to the same std::vector.
//      -> see ConcurrentStealAndProcess_NoDataRace (run under TSan).
//
//   2. WorkerThreads::executeJobs() never dispatches work onto the
//      std::thread objects in `threads` -- it calls ProcessJobs()
//      directly, synchronously, on the calling thread. The join loop
//      afterwards is a no-op because those threads were never started.
//      -> see ExecuteJobs_RunsOnWorkerThreads_NotCallingThread
//         (documented as an expected-failure test below).
//
// Build (example, adjust include paths / lib names as needed):
//   g++ -std=c++17 -pthread test_job_stealing.cpp JobQueue.cpp LocalJobQueue.cpp \
//       -lboost_unit_test_framework -o test_job_stealing
//
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
// JobQueue: single-threaded behavior
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(JobQueueBasicTests)

BOOST_AUTO_TEST_CASE(StartsEmpty)
{
    JobQueue queue;
    BOOST_TEST(queue.GetJobs().empty());
}

BOOST_AUTO_TEST_CASE(AddJob_IncreasesSize)
{
    JobQueue queue;
    queue.AddJobs([] {});
    BOOST_TEST(queue.GetJobs().size() == 1u);

    queue.AddJobs([] {});
    queue.AddJobs([] {});
    BOOST_TEST(queue.GetJobs().size() == 3u);
}

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
// JobQueue: concurrency
//
// CONFIRMED BUG: JobQueue::AddJob() is `jobQueue.push_back(job)` with no
// locking at all -- not even the LocalJobQueue's mtx guards this path,
// since AddJob() is defined on the base class and never overridden.
// Concurrent producers calling AddJob() race directly on the vector's
// internal state (size/capacity/reallocation). This test demonstrates
// that risk; run it under ThreadSanitizer (-fsanitize=thread) to see
// the race flagged directly rather than relying on it crashing.
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(JobQueueConcurrencyTests)

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
// LocalJobQueue: stealing behavior
//
// Real semantics from the .cpp:
//   - stealJobs() returns false outright if threadQueues.size() < 2.
//   - It scans threadQueues in index order, skipping `this`, and steals
//     from the FIRST non-empty queue it finds, popping its BACK element
//     (LIFO from the victim's perspective).
//   - ProcessJobs() consumes its own queue from the FRONT (FIFO), and
//     falls back to stealJobs() once its own queue is empty.
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(LocalJobQueueTests)

BOOST_AUTO_TEST_CASE(StealJobs_ReturnsFalse_WhenFewerThanTwoQueues)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    queues.push_back(std::make_unique<LocalJobQueue>());
    queues[0]->AddJobs([] {}); // even with work queued, size() < 2 short-circuits

    std::function<void()> stolen;
    BOOST_TEST(!queues[0]->stealJobs(queues, stolen));
}

BOOST_AUTO_TEST_CASE(StealJobs_ReturnsFalse_WhenAllOtherQueuesEmpty)
{
    std::vector<std::unique_ptr<LocalJobQueue>> queues;
    for (int i = 0; i < 3; ++i)
        queues.push_back(std::make_unique<LocalJobQueue>());

    std::function<void()> stolen;
    BOOST_TEST(!queues[0]->stealJobs(queues, stolen));
}

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
// KNOWN BUG: stealJobs() locks `this->mtx` instead of the victim's
// mutex. Concurrent thieves stealing from the same donor -- or a donor
// concurrently popping its own front() via ProcessJobs() -- race on
// the same std::vector<std::function<void()>> without real mutual
// exclusion. This test is a stress reproduction, best run with
// ThreadSanitizer (-fsanitize=thread) or Helgrind to surface the race
// directly; under a plain build it may pass "by luck" or may
// occasionally crash/corrupt depending on timing and allocator.
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
// WorkerThreads
//
// Real semantics from the .cpp:
//   - distributeJobsToLocalQueues() round-robins global jobs across
//     `threadsJobQueues` using `i % threads.size()`, then clears the
//     global queue.
//   - executeJobs() currently calls ProcessJobs() directly in a loop on
//     the calling thread -- it does NOT dispatch onto the std::thread
//     objects in `threads`. The join loop afterwards is a no-op since
//     those threads were never started. See the expected-failure test
//     below for a direct check of this.
// -----------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(WorkerThreadsTests)

BOOST_AUTO_TEST_CASE(Construction_CreatesQueuesMatchingThreadCount)
{
    if (std::thread::hardware_concurrency() < 3)
    {
        BOOST_TEST_MESSAGE("Skipping: not enough hardware threads to construct WorkerThreads safely.");
        return;
    }

    WorkerThreads pool;

    // NOTE: the constructor computes numThreads = hardware_concurrency() - 1,
    // then loops `for (i = 0; i < numThreads - 1; i++)`, so it ends up
    // creating (hardware_concurrency() - 2) entries, not (- 1). This
    // reflects the code AS WRITTEN; update if that off-by-one is fixed.
    unsigned int expected = std::thread::hardware_concurrency() >= 2
        ? std::thread::hardware_concurrency() - 2
        : 0;

    BOOST_TEST(pool.getThreads().size() == expected);
    BOOST_TEST(pool.getThreadsJobQueues().size() == expected);
}

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

    // This holds today even with the missing-thread-dispatch bug,
    // because ProcessJobs() still runs synchronously on the calling
    // thread -- the work gets done, just not in parallel.
    BOOST_TEST(ran.load() == totalJobs);
}

// -------------------------------------------------------------------
// KNOWN BUG, expected to currently FAIL:
// executeJobs() should hand each local queue's ProcessJobs() off to
// its corresponding std::thread in `threads`, so jobs run on worker
// threads distinct from the caller. Right now it calls ProcessJobs()
// directly in the loop on the calling thread, so every job's
// std::this_thread::get_id() equals the id of the thread that called
// executeJobs(). Once real dispatch is added, this test should pass.
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

    // Expected to fail until executeJobs() actually dispatches onto
    // worker threads instead of running ProcessJobs() inline.
    BOOST_TEST(ranOnDifferentThread.load());
}

BOOST_AUTO_TEST_SUITE_END()
