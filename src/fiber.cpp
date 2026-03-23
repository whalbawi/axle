#include "fiber.h"

#include <unistd.h>

#include <cstdint>

#include <functional>
#include <span>
#include <string>
#include <utility>

#include "fiber_arm64.h"
#include "sanitizer.h"

extern "C" {
void axle_fiber_swap(FiberCtx* src, const FiberCtx* dst);
}

namespace axle {

enum : uint8_t {
    FIBER_EXIT_CODE = 0x19,
};

Fiber::Fiber(std::string tag) : tag_(std::move(tag)) {
    AXLE_TSAN_CTX_INIT(true);
}

Fiber::Fiber(std::function<void()> func, std::string tag)
    : func_(std::move(func)), tag_(std::move(tag)) {
    fiber_ctx_init(&ctx_, std::span<uint8_t>(stack_), Fiber::run_func, this);
    AXLE_ASAN_CTX_INIT(stack_.data(), stack_.size());
    AXLE_TSAN_CTX_INIT(false);
}

Fiber::~Fiber() {
    AXLE_TSAN_CTX_FINI();
}

void Fiber::switch_to(Fiber* target) {
    AXLE_TSAN_SWITCH_TO_FIBER(this, target);
    AXLE_ASAN_START_SWITCH_FIBER(this, target);
    axle_fiber_swap(&ctx_, &target->ctx_);
    AXLE_ASAN_FINISH_SWITCH_FIBER(this);
}

void Fiber::interrupt() {
    interrupted_ = true;
}

bool Fiber::interrupted() {
    return std::exchange(interrupted_, false);
}

void Fiber::run_func(void* fiber_handle) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    Fiber* fiber = reinterpret_cast<Fiber*>(fiber_handle);
    AXLE_ASAN_FINISH_SWITCH_FIBER(fiber);
    fiber->func_();
    // Exit the program if the fiber's entry point ever returns.
    // Note the use of `_exit` and not `exit` because we don't want to run destructors on the wrong
    // fiber context.
    _exit(FIBER_EXIT_CODE);
}

} // namespace axle
