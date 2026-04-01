#include "fiber_x64.h"

#include <cstdint>

#include <span>

namespace {

void axle_fiber_run(void (*target)(void*), void* arg) {
    target(arg);
}

template <typename T>
uintptr_t to_uintptr_t(T arg) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<uintptr_t>(arg);
}

} // namespace

namespace axle {

void fiber_ctx_init(FiberCtx* ctx, std::span<uint8_t> stack, void (*target)(void*), void* arg) {
    ctx->rip = to_uintptr_t(axle_fiber_run);
    ctx->rdi = to_uintptr_t(target);
    ctx->rsi = to_uintptr_t(arg);

    constexpr uintptr_t stack_align = 16;
    constexpr uintptr_t ret_addr_sz = 8;
    ctx->rsp = (to_uintptr_t(stack.data() + stack.size()) & ~(stack_align - 1)) - ret_addr_sz;
}

} // namespace axle
