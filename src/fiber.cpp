#include "fiber.h"

#include <cstdint>

#include <functional>
#include <span>
#include <utility>

#include "fiber_arm64.h"

extern "C" {
void axle_fiber_swap(FiberCtx* src, const FiberCtx* dst);
}

namespace axle {

Fiber::Fiber(std::function<void()> func) : func_(std::move(func)) {
    fiber_ctx_init(&ctx_, std::span<uint8_t>(stack_), Fiber::run_func, this);
}

void Fiber::switch_to(const Fiber* other) {
    axle_fiber_swap(&ctx_, &other->ctx_);
}

void Fiber::run_func(void* fiber_handle) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    Fiber* fiber = reinterpret_cast<Fiber*>(fiber_handle);
    fiber->func_();
}

} // namespace axle
