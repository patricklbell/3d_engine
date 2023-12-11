#ifndef ENGINE_THREADPOOL_HPP
#define ENGINE_THREADPOOL_HPP

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>

struct ThreadPool {
    void start();
    void queueJob(const std::function<void()>& job);
    void stop();
    bool busy();
    void threadLoop();

    bool should_terminate = false;           // Tells threads to stop looking for jobs
    std::mutex queue_mutex;                  // Prevents data races to the job queue
    std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> jobs;
    std::atomic<int> jobs_in_progress = 0;
};

#endif // ENGINE_THREADPOOL_HPP
