// NOLINTBEGIN(readability-function-cognitive-complexity)
#include "axle/gate.h"

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "common.h"
#include "axle/scheduler.h"

#include "gtest/gtest.h"

#include "test_common.h"

namespace axle {
namespace {

TEST(GateTest, Works) {
    bool gate_passed = false;
    std::function<void()> entry_point = [&] {
        const Gate gate{};

        Scheduler::schedule([gate] { gate.post(); });

        AXLE_TEST_ASSERT_OK(gate.wait());
        gate_passed = true;
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_TRUE(gate_passed);
}

TEST(GateTest, DoubleWait) {
    int exception_cnt = 0;
    int tasks_run_cnt = 0;
    std::function<void()> entry_point = [&] {
        const Gate gate{};
        const Gate task_a_start{};
        const Gate task_b_start{};
        const Gate task_a_done{};
        const Gate task_b_done{};

        auto task = [&gate, &tasks_run_cnt, &exception_cnt](const Gate& task_start,
                                                            const Gate& task_done) {
            task_start.post();
            tasks_run_cnt++;
            if (tasks_run_cnt == 1) {
                AXLE_TEST_ASSERT_OK(gate.wait());
            } else {
                ASSERT_THROW(AXLE_UNUSED(gate.wait()), std::logic_error);
                exception_cnt++;
            }
            task_done.post();
        };
        Scheduler::schedule([&] { task(task_a_start, task_a_done); });
        Scheduler::schedule([&] { task(task_b_start, task_b_done); });

        AXLE_TEST_ASSERT_OK(task_a_start.wait());
        AXLE_TEST_ASSERT_OK(task_b_start.wait());
        gate.post();
        AXLE_TEST_ASSERT_OK(task_a_done.wait());
        AXLE_TEST_ASSERT_OK(task_b_done.wait());
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_EQ(2, tasks_run_cnt);
    ASSERT_EQ(1, exception_cnt);
}

TEST(GateTest, PostBeforeWait) {
    bool gate_passed = false;
    std::function<void()> entry_point = [&] {
        const Gate gate{};
        const Gate task_done{};

        Scheduler::schedule([&] {
            AXLE_TEST_ASSERT_OK(gate.wait());
            gate_passed = true;
            task_done.post();
        });

        gate.post();
        AXLE_TEST_ASSERT_OK(task_done.wait());
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_TRUE(gate_passed);
}

TEST(GateTest, DoublePostIsNoop) {
    std::function<void()> entry_point = [&] {
        const Gate gate{};
        const Gate task_done{};

        gate.post();
        gate.post();
        AXLE_TEST_ASSERT_OK(gate.wait());
    };

    Scheduler::enter(std::move(entry_point));
}

TEST(GateTest, PostBeforeWaitSamefiber) {
    bool gate_passed = false;
    std::function<void()> entry_point = [&] {
        const Gate gate{};

        gate.post();
        AXLE_TEST_ASSERT_OK(gate.wait());
        gate_passed = true;
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_TRUE(gate_passed);
}

TEST(GateTest, Reuse) {
    int wait_cnt = 0;
    std::function<void()> entry_point = [&] {
        const Gate gate{};
        const Gate gate_1{};
        const Gate gate_2{};

        Scheduler::schedule([&] {
            AXLE_TEST_ASSERT_OK(gate.wait());
            wait_cnt++;
            gate_1.post();

            AXLE_TEST_ASSERT_OK(gate.wait());
            wait_cnt++;
            gate_2.post();
        });

        gate.post();
        AXLE_TEST_ASSERT_OK(gate_1.wait());
        gate.post();
        AXLE_TEST_ASSERT_OK(gate_2.wait());
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_EQ(2, wait_cnt);
}

TEST(GateTest, ReusePostBeforeWait) {
    int wait_cnt = 0;
    std::function<void()> entry_point = [&] {
        const Gate gate{};

        gate.post();
        AXLE_TEST_ASSERT_OK(gate.wait());
        wait_cnt++;

        gate.post();
        AXLE_TEST_ASSERT_OK(gate.wait());
        wait_cnt++;
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_EQ(2, wait_cnt);
}

TEST(GateTest, ChainedGates) {
    std::vector<int> order;
    std::function<void()> entry_point = [&] {
        const Gate a{};
        const Gate b{};
        const Gate c{};
        const Gate done{};

        Scheduler::schedule([&] {
            AXLE_TEST_ASSERT_OK(a.wait());
            order.push_back(1);
            b.post();
        });
        Scheduler::schedule([&] {
            AXLE_TEST_ASSERT_OK(b.wait());
            order.push_back(2);
            c.post();
        });
        Scheduler::schedule([&] {
            AXLE_TEST_ASSERT_OK(c.wait());
            order.push_back(3);
            done.post();
        });

        a.post();
        AXLE_TEST_ASSERT_OK(done.wait());
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_EQ(3, order.size());
    ASSERT_EQ(1, order[0]);
    ASSERT_EQ(2, order[1]);
    ASSERT_EQ(3, order[2]);
}

TEST(GateTest, MoveConstruct) {
    bool gate_passed = false;
    std::function<void()> entry_point = [&] {
        Gate original{};
        const Gate gate{std::move(original)};
        const Gate task_done{};

        Scheduler::schedule([&] {
            gate.post();
            task_done.post();
        });

        AXLE_TEST_ASSERT_OK(gate.wait());
        gate_passed = true;
        AXLE_TEST_ASSERT_OK(task_done.wait());
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_TRUE(gate_passed);
}

TEST(GateTest, WaitReturnsErrOnShutdown) {
    const Gate init{};
    const Gate gate{};
    Scheduler::enter([&] {
        Scheduler::schedule([&] {
            init.post();
            // Runs in the context of `fini` which interrupts the fiber.
            AXLE_TEST_ASSERT_ERR(gate.wait(), "(nil)");
        });
        AXLE_TEST_ASSERT_OK(init.wait());
    });
}

TEST(GateTest, PingPong) {
    constexpr int rounds_cnt = 100;
    int round_cnt = 0;
    std::function<void()> entry_point = [&] {
        const Gate ping{};
        const Gate pong{};
        const Gate done{};

        Scheduler::schedule([&] {
            for (int i = 0; i < rounds_cnt; i++) {
                AXLE_TEST_ASSERT_OK(ping.wait());
                round_cnt++;
                pong.post();
            }
            done.post();
        });

        for (int i = 0; i < rounds_cnt; i++) {
            ping.post();
            AXLE_TEST_ASSERT_OK(pong.wait());
        }
        AXLE_TEST_ASSERT_OK(done.wait());
    };

    Scheduler::enter(std::move(entry_point));
    ASSERT_EQ(rounds_cnt, round_cnt);
}

} // namespace
} // namespace axle
// NOLINTEND(readability-function-cognitive-complexity)
