#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>

#include "fiber_arm64.h"

namespace axle {

class Fiber {
  public:
    Fiber() = default;
    explicit Fiber(std::function<void()> func);
    void switch_to(const Fiber* other);

  private:
    static void run_func(void* fiber_handle);
    static constexpr size_t k_stack_sz = 4096;

    std::function<void()> func_;
    FiberCtx ctx_{};
    std::array<uint8_t, k_stack_sz> stack_{};
};

} // namespace axle
