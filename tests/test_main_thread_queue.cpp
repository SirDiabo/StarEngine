#include "TestHelper.h"
#include "Core/MainThreadQueue.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

void PumpUntil(const std::atomic<bool>& done, double limitSeconds = 10.0) {
    const auto start = std::chrono::steady_clock::now();
    while (!done.load()) {
        MainThreadQueue::Pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed.count() > limitSeconds) break;
    }
}

}

TEST(main_thread_queue_refuses_work_before_a_main_thread_is_marked) {
    std::atomic<bool> done{false};
    bool ran = true;
    bool executed = false;
    std::thread worker([&]() {
        ran = MainThreadQueue::Run([&]() { executed = true; });
        done = true;
    });
    PumpUntil(done, 2.0);
    worker.join();
    CHECK(!ran);
    CHECK(!executed);
}

TEST(main_thread_queue_runs_worker_requests_on_the_main_thread) {
    MainThreadQueue::MarkMainThread();
    CHECK(MainThreadQueue::IsMainThread());

    const std::thread::id mainId = std::this_thread::get_id();
    std::thread::id ranOn;
    bool ran = false;
    std::atomic<bool> done{false};
    std::thread worker([&]() {
        CHECK(!MainThreadQueue::IsMainThread());
        ran = MainThreadQueue::Run([&]() { ranOn = std::this_thread::get_id(); });
        done = true;
    });
    PumpUntil(done);
    worker.join();
    CHECK(ran);
    CHECK(ranOn == mainId);
}

TEST(main_thread_queue_runs_main_thread_calls_inline) {
    MainThreadQueue::MarkMainThread();
    int value = 0;
    CHECK(MainThreadQueue::Run([&]() { value = 5; }));
    CHECK(value == 5);
    CHECK(!MainThreadQueue::Run(std::function<void()>()));
}

TEST(main_thread_queue_runs_every_task_exactly_once) {
    MainThreadQueue::MarkMainThread();
    constexpr int kThreads = 8;
    constexpr int kTasks = 60;
    std::vector<int> counts(kThreads * kTasks, 0);
    std::atomic<int> finished{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < kThreads; t++) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < kTasks; i++) {
                const int index = t * kTasks + i;
                if (!MainThreadQueue::Run([&, index]() { counts[(size_t)index]++; })) failures++;
            }
            finished++;
        });
    }
    std::atomic<bool> all{false};
    std::thread watcher([&]() {
        while (finished.load() < kThreads) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        all = true;
    });
    PumpUntil(all);
    watcher.join();
    for (std::thread& w : workers) w.join();

    CHECK(failures.load() == 0);
    for (int count : counts) CHECK(count == 1);
}

TEST(main_thread_queue_keeps_each_workers_tasks_in_order) {
    MainThreadQueue::MarkMainThread();
    std::vector<int> order;
    std::atomic<bool> done{false};
    std::thread worker([&]() {
        for (int i = 0; i < 25; i++) MainThreadQueue::Run([&, i]() { order.push_back(i); });
        done = true;
    });
    PumpUntil(done);
    worker.join();
    REQUIRE(order.size() == 25);
    for (int i = 0; i < 25; i++) CHECK(order[(size_t)i] == i);
}

TEST(main_thread_queue_pump_does_nothing_when_idle) {
    MainThreadQueue::MarkMainThread();
    MainThreadQueue::Pump();
    MainThreadQueue::Pump(0.0);
    CHECK(true);
}

TEST(main_thread_queue_shutdown_releases_waiting_workers) {
    MainThreadQueue::MarkMainThread();
    std::atomic<int> started{0};
    std::atomic<int> returnedFalse{0};
    std::atomic<int> executed{0};
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; i++) {
        workers.emplace_back([&]() {
            started++;
            if (!MainThreadQueue::Run([&]() { executed++; })) returnedFalse++;
        });
    }
    while (started.load() < 4) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    MainThreadQueue::Shutdown();
    for (std::thread& w : workers) w.join();

    CHECK(returnedFalse.load() == 4);
    CHECK(executed.load() == 0);

    std::atomic<bool> done{false};
    bool ran = true;
    std::thread late([&]() {
        ran = MainThreadQueue::Run([]() {});
        done = true;
    });
    late.join();
    CHECK(!ran);

    MainThreadQueue::MarkMainThread();
    std::atomic<bool> again{false};
    bool ranAgain = false;
    std::thread revived([&]() {
        ranAgain = MainThreadQueue::Run([]() {});
        again = true;
    });
    PumpUntil(again);
    revived.join();
    CHECK(ranAgain);
}

TEST_MAIN()
