// NOLINTBEGIN(readability-function-cognitive-complexity)

#include "fiber.h"

#include "gtest/gtest.h"

namespace axle {

TEST(FiberTest, PingPong) {
    Fiber main_fiber{};
    int a = 0;
    Fiber other_fiber{[&] {
        EXPECT_EQ(0, a);
        a++;
        other_fiber.switch_to(&main_fiber);
        EXPECT_EQ(2, a);
        a++;
        other_fiber.switch_to(&main_fiber);
    }};
    main_fiber.switch_to(&other_fiber);
    EXPECT_EQ(1, a);
    a++;
    main_fiber.switch_to(&other_fiber);
    EXPECT_EQ(3, a);
}

TEST(FiberTest, ExitFromFiber) {
    Fiber main_fiber{};
    int a = 1;

    Fiber fiber1{[&] {
        EXPECT_EQ(1, a);
        a = 2;
        return;
    }};

    EXPECT_EXIT(main_fiber.switch_to(&fiber1), testing::ExitedWithCode(0x19), "");
}

TEST(FiberTest, Chain) {
    Fiber main_fiber{};
    int a = 0;
    Fiber fiber2{[&] {
        a = 2;
        fiber2.switch_to(&main_fiber);
    }};
    Fiber fiber1{[&] {
        EXPECT_EQ(0, a);
        a = 1;
        fiber1.switch_to(&fiber2);
    }};
    main_fiber.switch_to(&fiber1);
    EXPECT_EQ(2, a);
}

TEST(FiberTest, Interrupt) {
    Fiber fiber{};
    ASSERT_FALSE(fiber.interrupted());

    fiber.interrupt();
    ASSERT_TRUE(fiber.interrupted());
    // Fiber::interrupted resets the interrupt state
    ASSERT_FALSE(fiber.interrupted());
}

} // namespace axle

// NOLINTEND(readability-function-cognitive-complexity)
