#pragma once

#include <cstddef>
#include <cstdint>

#include <span>

#include "fiber_regoff_arm64.h"

struct FiberCtx {
    uintptr_t r0;
    uintptr_t r1;

    // special purpose registers
    uintptr_t r16;
    uintptr_t r17;
    uintptr_t r18; // platform specific (maybe inter-procedural state)

    // callee-saved registers
    uintptr_t r19;
    uintptr_t r20;
    uintptr_t r21;
    uintptr_t r22;
    uintptr_t r23;
    uintptr_t r24;
    uintptr_t r25;
    uintptr_t r26;
    uintptr_t r27;
    uintptr_t r28;
    uintptr_t r29;

    uintptr_t v8;
    uintptr_t v9;
    uintptr_t v10;
    uintptr_t v11;
    uintptr_t v12;
    uintptr_t v13;
    uintptr_t v14;
    uintptr_t v15;

    uintptr_t SP; // stack pointer
    uintptr_t LR; // link register
};

// clang-format off
static_assert(offsetof(FiberCtx,  r0) == AXLE_REG_r0,  "Bad register offset");
static_assert(offsetof(FiberCtx,  r1) == AXLE_REG_r1,  "Bad register offset");
static_assert(offsetof(FiberCtx, r16) == AXLE_REG_r16, "Bad register offset");
static_assert(offsetof(FiberCtx, r17) == AXLE_REG_r17, "Bad register offset");
static_assert(offsetof(FiberCtx, r18) == AXLE_REG_r18, "Bad register offset");
static_assert(offsetof(FiberCtx, r19) == AXLE_REG_r19, "Bad register offset");
static_assert(offsetof(FiberCtx, r20) == AXLE_REG_r20, "Bad register offset");
static_assert(offsetof(FiberCtx, r21) == AXLE_REG_r21, "Bad register offset");
static_assert(offsetof(FiberCtx, r22) == AXLE_REG_r22, "Bad register offset");
static_assert(offsetof(FiberCtx, r23) == AXLE_REG_r23, "Bad register offset");
static_assert(offsetof(FiberCtx, r24) == AXLE_REG_r24, "Bad register offset");
static_assert(offsetof(FiberCtx, r25) == AXLE_REG_r25, "Bad register offset");
static_assert(offsetof(FiberCtx, r26) == AXLE_REG_r26, "Bad register offset");
static_assert(offsetof(FiberCtx, r27) == AXLE_REG_r27, "Bad register offset");
static_assert(offsetof(FiberCtx, r28) == AXLE_REG_r28, "Bad register offset");
static_assert(offsetof(FiberCtx, r29) == AXLE_REG_r29, "Bad register offset");
static_assert(offsetof(FiberCtx,  v8) == AXLE_REG_v8,  "Bad register offset");
static_assert(offsetof(FiberCtx,  v9) == AXLE_REG_v9,  "Bad register offset");
static_assert(offsetof(FiberCtx, v10) == AXLE_REG_v10, "Bad register offset");
static_assert(offsetof(FiberCtx, v11) == AXLE_REG_v11, "Bad register offset");
static_assert(offsetof(FiberCtx, v12) == AXLE_REG_v12, "Bad register offset");
static_assert(offsetof(FiberCtx, v13) == AXLE_REG_v13, "Bad register offset");
static_assert(offsetof(FiberCtx, v14) == AXLE_REG_v14, "Bad register offset");
static_assert(offsetof(FiberCtx, v15) == AXLE_REG_v15, "Bad register offset");
static_assert(offsetof(FiberCtx,  SP) == AXLE_REG_SP,  "Bad register offset");
static_assert(offsetof(FiberCtx,  LR) == AXLE_REG_LR,  "Bad register offset");
// clang-format on

namespace axle {

void fiber_ctx_init(FiberCtx* ctx, std::span<uint8_t> stack, void (*target)(void*), void* arg);

} // namespace axle
