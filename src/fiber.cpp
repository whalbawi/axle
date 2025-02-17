#include "fiber.h"

#include <cstdint>

#include <functional>
#include <span>
#include <utility>

#include "fiber_arm64.h"
#include "sanitizer.h"

extern "C" {
void axle_fiber_swap(FiberCtx* src, const FiberCtx* dst);
}

namespace axle {

Fiber::Fiber() {
    AXLE_TSAN_CTX_INIT(true);
}

Fiber::Fiber(std::function<void()> func) : func_(std::move(func)) {
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
}

} // namespace axle
