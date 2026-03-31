#pragma once

#include <algorithm>
#include <random>
#include <span>

#ifndef AXLE_TEST_RNG_SEED
#define AXLE_TEST_RNG_SEED 42
#endif

#define AXLE_TEST_ASSERT_OK(status)                                                                \
    {                                                                                              \
        auto&& status__ = (status);                                                                \
        if (status__.is_err()) {                                                                   \
            FAIL() << #status << " is not OK - error=" << status__.err();                          \
        }                                                                                          \
    }

#define AXLE_TEST_ASSERT_ERR(status, errval)                                                       \
    {                                                                                              \
        auto&& status__ = (status);                                                                \
        if (status__.is_ok()) {                                                                    \
            FAIL() << #status << " is not ERR";                                                    \
        }                                                                                          \
        ASSERT_EQ((errval), status__.err());                                                       \
    }

template <typename C>
concept Iterable = requires(C& c) {
    std::begin(c);
    std::end(c);
};

template <Iterable C>
inline void rnd_buf(C& c) {
    using T = C::value_type;
    constexpr T seed = AXLE_TEST_RNG_SEED;

    static thread_local std::mt19937 rng{seed};
    std::uniform_int_distribution<T> dist;
    std::generate(std::begin(c), std::end(c), [&] { return dist(rng); });
}
