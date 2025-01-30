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
    Fiber() = default;
    explicit Fiber(std::function<void()> func);

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
};

} // namespace axle
