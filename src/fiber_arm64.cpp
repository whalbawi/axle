#include "fiber_arm64.h"

#include <cstddef>
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
    ctx->r0 = to_uintptr_t(target);
    ctx->r1 = to_uintptr_t(arg);
    ctx->LR = to_uintptr_t(axle_fiber_run);

    const uintptr_t alignment = 15;
    ctx->SP = to_uintptr_t(stack.data() + stack.size()) & ~alignment;
}

} // namespace axle
