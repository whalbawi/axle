// NOLINTBEGIN(readability-function-cognitive-complexity)

#include "axle/scheduler.h"

#include <csignal>

#include <exception>
#include <stdexcept>
#include <thread>

#include "fiber.h" // IWYU pragma: keep

#include "gtest/gtest.h"

namespace axle {

namespace {

template <typename T>
void maybe_shutdown(const T& a, const T& a_lim) {
    if (a >= a_lim) {
        Scheduler::shutdown();
    }
};

} // namespace

TEST(SchedulerTest, EnterRunsEntryPoint) {
    bool ran = false;
    Scheduler::enter([&] { ran = true; });
    ASSERT_TRUE(ran);
}

TEST(SchedulerTest, EnterFromMultipleThreads) {
    int a = 0;
    int b = 0;

    std::thread t1([&] { Scheduler::enter([&] { a = 1; }); });
    std::thread t2([&] { Scheduler::enter([&] { b = 2; }); });

    t1.join();
    t2.join();

    ASSERT_EQ(1, a);
    ASSERT_EQ(2, b);
}

TEST(SchedulerTest, EnterNestedScheduling) {
    bool entry_point_ran = false;
    bool entry_point_eintr = false;
    bool inner_ran = false;
    bool inner_eintr = false;

    Scheduler::enter([&] {
        Scheduler::schedule(
            [&] {
                inner_ran = true;
                inner_eintr = Scheduler::current_fiber()->interrupted();
            },
            "inner");
        entry_point_ran = true;
        entry_point_eintr = Scheduler::current_fiber()->interrupted();
    });
    ASSERT_TRUE(entry_point_ran);
    ASSERT_FALSE(entry_point_eintr);
    ASSERT_TRUE(inner_ran);
    ASSERT_FALSE(inner_eintr);
}

TEST(SchedulerTest, EnterDoubleNesting) {
    bool entry_point_ran = false;
    bool entry_point_eintr = false;
    bool outer_ran = false;
    bool outer_eintr = false;
    bool inner_ran = false;

    Scheduler::enter([&] {
        Scheduler::schedule(
            [&] {
                outer_ran = true;
                outer_eintr = Scheduler::current_fiber()->interrupted();
                ASSERT_THROW(Scheduler::schedule([&] { inner_ran = true; }, "inner"),
                             std::runtime_error);
            },
            "outer");
        entry_point_ran = true;
        entry_point_eintr = Scheduler::current_fiber()->interrupted();
    });

    ASSERT_TRUE(entry_point_ran);
    ASSERT_FALSE(entry_point_eintr);
    ASSERT_TRUE(outer_ran);
    ASSERT_FALSE(outer_eintr);
    ASSERT_FALSE(inner_ran);
}

TEST(SchedulerTest, EnterAfterSchedule) {
    bool adhoc_ran = false;
    bool entry_point_ran = false;

    Scheduler::init();
    Scheduler::schedule([&] { adhoc_ran = true; }, "adhoc");
    ASSERT_THROW(Scheduler::enter([&] { entry_point_ran = true; }), std::runtime_error);
    ASSERT_FALSE(adhoc_ran); // Doesn't run in the context of `Scheduler::enter`
    Scheduler::fini();

    ASSERT_FALSE(entry_point_ran);
    ASSERT_TRUE(adhoc_ran); // Runs in the context of the `Scheduler::fini`
}

TEST(SchedulerTest, InitWorks) {
    ASSERT_NO_THROW(Scheduler::init());
    Scheduler::fini();
}

TEST(SchedulerTest, InitTwiceFails) {
    Scheduler::init();
    ASSERT_THROW(Scheduler::init(), std::runtime_error);
    Scheduler::fini();
}

TEST(SchedulerTest, FiniWorks) {
    Scheduler::init();
    ASSERT_NO_THROW(Scheduler::fini());
}

TEST(SchedulerTest, FiniWithoutInit) {
    ASSERT_THROW(Scheduler::fini(), std::runtime_error);
}

TEST(SchedulerTest, FiniTwice) {
    Scheduler::init();
    ASSERT_NO_THROW(Scheduler::fini());
    ASSERT_THROW(Scheduler::fini(), std::runtime_error);
}

TEST(SchedulerTest, ScheduleWorks) {
    int a = 0;
    Scheduler::init();

    Scheduler::schedule([&] {
        a += 1;
        maybe_shutdown(a, 3);
    });

    Scheduler::schedule([&] {
        a += 2;
        maybe_shutdown(a, 3);
    });

    Scheduler::run();
    Scheduler::fini();

    ASSERT_EQ(3, a);
}

TEST(SchedulerTest, ScheduleNoInit) {
    ASSERT_THROW(Scheduler::schedule([] {}), std::runtime_error);
}

TEST(SchedulerTest, ScheduleNested) {
    int a = 0;
    Scheduler::init();

    Scheduler::schedule([&] {
        a++;
        Scheduler::schedule([&a] {
            a++;
            maybe_shutdown(a, 0);
        });
    });

    Scheduler::run();
    Scheduler::fini();

    ASSERT_EQ(2, a);
}

TEST(SchedulerTest, ScheduleManyTasks) {
    constexpr int task_cnt = 10000;
    int a = 0;
    Scheduler::init();

    for (int i = 0; i < task_cnt; ++i) {
        Scheduler::schedule([&] {
            a++;
            maybe_shutdown(a, task_cnt);
        });
    }

    Scheduler::run();
    Scheduler::fini();

    ASSERT_EQ(task_cnt, a);
}

TEST(SchedulerTest, ScheduleMultithreaded) {
    constexpr int task_cnt_a = 10000;
    constexpr int task_cnt_b = 5000;
    int a = 0;
    int b = 0;

    std::thread thread_a = std::thread([&] {
        Scheduler::init();
        for (int i = 0; i < task_cnt_a; ++i) {
            Scheduler::schedule([&] {
                a++;
                maybe_shutdown(a, task_cnt_a);
            });
        }

        Scheduler::run();
        Scheduler::fini();
    });

    std::thread thread_b = std::thread([&] {
        Scheduler::init();
        for (int i = 0; i < task_cnt_b; ++i) {
            Scheduler::schedule([&] {
                b++;
                maybe_shutdown(b, task_cnt_b);
            });
        }

        Scheduler::run();
        Scheduler::fini();
    });

    thread_a.join();
    thread_b.join();

    ASSERT_EQ(task_cnt_a, a);
    ASSERT_EQ(task_cnt_b, b);
}

TEST(SchedulerTest, ScheduleTaskThrows) {
    ASSERT_EXIT(
        {
            Scheduler::init();
            Scheduler::schedule([] { throw std::exception{}; }, "my-task");
            Scheduler::run();
            Scheduler::fini();
        },
        testing::KilledBySignal(SIGABRT),
        ".*");
}

TEST(SchedulerTest, YieldBeforeRun) {
    int a = 0;
    Scheduler::init();

    Scheduler::schedule([&] {
        a++;
        maybe_shutdown(a, 0);
    });

    ASSERT_THROW(Scheduler::yield(), std::runtime_error);
    Scheduler::fini();
}

// TODO (whalbawi): Enable this test once yielding pushes the fiber back into the ready queue
TEST(SchedulerTest, DISABLED_YieldWorks) {
    int a = 0;
    int b = 0;
    Scheduler::init();

    Scheduler::schedule([&] {
        a++;
        ASSERT_EQ(1, a);
        Scheduler::yield();
        a++;
    });

    Scheduler::schedule([&] {
        b++;
        ASSERT_EQ(1, b);
        Scheduler::yield();
        b++;
        Scheduler::shutdown();
    });

    Scheduler::run();

    ASSERT_EQ(2, a);
    ASSERT_EQ(2, b);
    Scheduler::fini();
}

} // namespace axle

// NOLINTEND(readability-function-cognitive-complexity)
