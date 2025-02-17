#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>

#include "fiber_arm64.h"
#include "sanitizer.h"

namespace axle {

class Fiber {
  public:
    Fiber();
    explicit Fiber(std::function<void()> func);
    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;
    Fiber(Fiber&&) = delete;
    Fiber& operator=(Fiber&&) = delete;
    ~Fiber();

    void switch_to(Fiber* target);
    void interrupt();
    bool interrupted();

  private:
    static constexpr size_t k_stack_sz = 32 * 1024UL;

    static void run_func(void* fiber_handle);

    std::function<void()> func_;
    FiberCtx ctx_{};
    std::array<uint8_t, k_stack_sz> stack_{};
    bool interrupted_{false};

    AXLE_ASAN_CTX_DECLARE();
    AXLE_TSAN_CTX_DECLARE();
};

} // namespace axle
