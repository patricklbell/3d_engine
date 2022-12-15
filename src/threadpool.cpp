#include <thread>
#include <functional>

#include "threadpool.hpp"

void ThreadPool::start() {
    const uint32_t num_threads = std::max((int)std::thread::hardware_concurrency() - 2, 4);
    threads.resize(num_threads);
    for (uint32_t i = 0; i < num_threads; i++) {
        // @note This lambda may be unnecessary
        threads.at(i) = std::thread(&ThreadPool::threadLoop, this);
    }
}

void ThreadPool::threadLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            mutex_condition.wait(lock, [this] {
                return !jobs.empty() || should_terminate;
                });
            if (should_terminate) {
                return;
            }
            job = jobs.front();
            jobs_in_progress++;
            jobs.pop();
        }
        job();
        jobs_in_progress--;
    }
}

void ThreadPool::queueJob(const std::function<void()>& job) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        jobs.push(job);
    }
    mutex_condition.notify_one();
}

bool ThreadPool::busy() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return !(jobs.empty() && jobs_in_progress == 0);
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        should_terminate = true;
    }
    mutex_condition.notify_all();
    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}
