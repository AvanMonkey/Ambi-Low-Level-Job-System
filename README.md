<h1 align="center">Ambi</h1>

<p align="center"> A lightweight C++ job system built around work stealing. </p>

<p align="center"> <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++17"> <img src="https://img.shields.io/badge/Status-Active_Development-purple" alt="Status"> </p>

<hr>

<h2>About</h2>

<p> <b>Ambi</b> is a lightweight C++ job system designed to efficiently distribute work across multiple worker threads using a work-stealing scheduler </p>

<p> Each worker thread has its own local job queue. When a worker runs out of work, it can attempt to steal jobs from another worker's queue, helping to reduce idle time and improve processing times. </p>

<h2>Features</h2>

<ul> <li>Multithreaded job execution</li> <li>Job stealing between worker threads</li> <li>Local job queues for individual workers</li> <li>Thread-safe job processing</li> <li>Dynamic workload distribution</li> <li>Faster Execution times</ul>

<h2> How It Works</h2>

<p> Ambi uses multiple worker threads to process jobs concurrently. Each worker has access to its own local queue of jobs. </p>

<p> When a worker finishes all of its assigned work, it can attempt to steal jobs from another worker's queue to help reduce backlog. </p>

<h2>Core Components</h2>

<h3>JobQueue</h3>

<p> Stores jobs that are waiting to be processed. </p>

<h3>LocalJobQueue</h3>

<p> A worker-specific queue that allows workers to process their own jobs and steal work from other queues when they become idle. </p>

<h3>WorkerThreads</h3>

<p> Manages the worker threads responsible for executing jobs. </p>

<h2>Example</h2>

<pre> <code> jobQueue.AddJob([]() { // Perform work }); </code> </pre>

<p> Worker threads can then process the submitted jobs concurrently. </p>

<h2>Project Goals</h2>

<ul> <li>Explore multithreaded programming in C++</li> <li>Develop a reusable job system</li> <li>Improve workload distribution across worker threads</li> <li>Reduce worker idle time using work stealing</li> <li>Learn more about concurrent programming and thread synchronisation</li> </ul>

<h2>Technologies</h2>

<ul> <li><b>C++</b></li> <li><code>std::thread</code></li> <li><code>std::mutex</code></li> <li><code>std::function</code></li> <li><code>std::vector</code></li> </ul>

<h2>Project Status</h2>

<p> Ambi is planned to have further updates </p>

<p> It will be used in future projects and any bugs found will be fixed ASAP </p>

<hr>

<p align="center"> Ambi is licensed under the MIT License. </p> </div>
