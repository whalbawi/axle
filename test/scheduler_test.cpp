// NOLINTBEGIN(readability-function-cognitive-complexity)

#include "axle/scheduler.h"

#include <thread>

#include "gtest/gtest.h"

namespace axle {

TEST(SchedulerTest, RunTasks) {
    int a = 0;
    Scheduler::init();

    Scheduler::schedule([&] { a += 1; });

    Scheduler::schedule([&] { a += 2; });

    Scheduler::stop();
    Scheduler::yield();
    Scheduler::fini();

    ASSERT_EQ(3, a);
}

TEST(SchedulerTest, NoInit) {
    ASSERT_DEATH(Scheduler::schedule([] {}), "");
}

TEST(SchedulerTest, NestedScheduling) {
    int a = 0;
    Scheduler::init();

    Scheduler::schedule([&] {
        a++;
        Scheduler::schedule([&a] { a++; });
    });

    Scheduler::stop();
    Scheduler::yield();
    Scheduler::fini();

    ASSERT_EQ(2, a);
}

TEST(SchedulerTest, ManyTasks) {
    constexpr int task_cnt = 10000;
    int a = 0;
    Scheduler::init();

    for (int i = 0; i < task_cnt; ++i) {
        Scheduler::schedule([&] { a++; });
    }

    Scheduler::stop();
    Scheduler::yield();
    Scheduler::fini();

    ASSERT_EQ(task_cnt, a);
}

TEST(SchedulerTest, Multithreaded) {
    constexpr int task_cnt_a = 10000;
    constexpr int task_cnt_b = 5000;
    int a = 0;
    int b = 0;

    std::thread thread_a = std::thread([&] {
        Scheduler::init();
        for (int i = 0; i < task_cnt_a; ++i) {
            Scheduler::schedule([&] { a++; });
        }

        Scheduler::stop();
        Scheduler::yield();
        Scheduler::fini();
    });

    std::thread thread_b = std::thread([&] {
        Scheduler::init();
        for (int i = 0; i < task_cnt_b; ++i) {
            Scheduler::schedule([&] { b++; });
        }

        Scheduler::stop();
        Scheduler::yield();
        Scheduler::fini();
    });

    thread_a.join();
    thread_b.join();

    ASSERT_EQ(task_cnt_a, a);
    ASSERT_EQ(task_cnt_b, b);
}

} // namespace axle

// NOLINTEND(readability-function-cognitive-complexity)
