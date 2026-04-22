#pragma once

#include <cstdint>

#include <span>

#if defined(__aarch64__)
#include "fiber_arm64.h" // IWYU pragma: export -- axle::FiberCtx
#elif defined(__x86_64__)
#include "fiber_x64.h" // IWYU pragma: export -- axle::FiberCtx
#endif                 // defined(__aarch64__)

namespace axle {

void fiber_ctx_init(FiberCtx* ctx, std::span<uint8_t> stack, void (*target)(void*), void* arg);

} // namespace axle
