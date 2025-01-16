#include "fiber.h"

#include "gtest/gtest.h"

TEST(Fibers, PingPong) {
    axle::Fiber main_fiber{};
    int a = 0;
    axle::Fiber other_fiber{[&] {
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

TEST(Fibers, Chain) {
    axle::Fiber main_fiber{};
    int a = 0;
    axle::Fiber fiber2{[&] {
        a = 2;
        fiber2.switch_to(&main_fiber);
    }};
    axle::Fiber fiber1{[&] {
        EXPECT_EQ(0, a);
        a = 1;
        fiber1.switch_to(&fiber2);
    }};
    main_fiber.switch_to(&fiber1);
    EXPECT_EQ(2, a);
}
