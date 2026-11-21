#include "MainThreadQueue.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace {

struct Task {
    std::function<void()> work;
    bool done = false;
    bool executed = false;
};

std::mutex lock;
std::condition_variable finished;
std::deque<std::shared_ptr<Task>> pending;
std::thread::id mainThread;
bool mainThreadKnown = false;
bool shuttingDown = false;

}

void MainThreadQueue::MarkMainThread() {
    std::lock_guard<std::mutex> guard(lock);
    mainThread = std::this_thread::get_id();
    mainThreadKnown = true;
    shuttingDown = false;
}

bool MainThreadQueue::IsMainThread() {
    std::lock_guard<std::mutex> guard(lock);
    return mainThreadKnown && mainThread == std::this_thread::get_id();
}

bool MainThreadQueue::Run(const std::function<void()>& work) {
    if (!work) return false;
    if (IsMainThread()) {
        work();
        return true;
    }

    auto task = std::make_shared<Task>();
    task->work = work;
    std::unique_lock<std::mutex> guard(lock);
    if (shuttingDown || !mainThreadKnown) return false;
    pending.push_back(task);
    finished.wait(guard, [&]() { return task->done; });
    return task->executed;
}

void MainThreadQueue::Pump(double maxSeconds) {
    const auto start = std::chrono::steady_clock::now();
    bool first = true;
    for (;;) {
        std::shared_ptr<Task> task;
        {
            std::lock_guard<std::mutex> guard(lock);
            if (pending.empty()) return;
            if (!first) {
                const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
                if (elapsed.count() >= maxSeconds) return;
            }
            task = pending.front();
            pending.pop_front();
        }
        first = false;

        task->work();

        {
            std::lock_guard<std::mutex> guard(lock);
            task->executed = true;
            task->done = true;
        }
        finished.notify_all();
    }
}

void MainThreadQueue::Shutdown() {
    std::deque<std::shared_ptr<Task>> abandoned;
    {
        std::lock_guard<std::mutex> guard(lock);
        shuttingDown = true;
        abandoned.swap(pending);
        for (auto& task : abandoned) task->done = true;
    }
    finished.notify_all();
}
