#pragma once

#include <cstddef>
#include <cstdint>

#include <span>

#include "fiber_reg_off_arm64.h"

struct FiberCtx {
    uintptr_t x0; // Holds source fiber
    uintptr_t x1; // Holds destination fiber

    // special platform registers
    uintptr_t x16;
    uintptr_t x17;
    uintptr_t x18;

    // callee-saved registers
    uintptr_t x19;
    uintptr_t x20;
    uintptr_t x21;
    uintptr_t x22;
    uintptr_t x23;
    uintptr_t x24;
    uintptr_t x25;
    uintptr_t x26;
    uintptr_t x27;
    uintptr_t x28;
    uintptr_t x29;

    // floating-point/SIMD registers
    // TODO(whalbawi): Should we capture all 128 bits?
    uintptr_t d8;
    uintptr_t d9;
    uintptr_t d10;
    uintptr_t d11;
    uintptr_t d12;
    uintptr_t d13;
    uintptr_t d14;
    uintptr_t d15;

    uintptr_t sp; // stack pointer
    uintptr_t lr; // link register
};

// clang-format off
static_assert(offsetof(FiberCtx,  x0) == AXLE_REG_OFF_ARM64_X0,  "bad offset for register x0");
static_assert(offsetof(FiberCtx,  x1) == AXLE_REG_OFF_ARM64_X1,  "bad offset for register x1");
static_assert(offsetof(FiberCtx, x16) == AXLE_REG_OFF_ARM64_X16, "bad offset for register x16");
static_assert(offsetof(FiberCtx, x17) == AXLE_REG_OFF_ARM64_X17, "bad offset for register x17");
static_assert(offsetof(FiberCtx, x18) == AXLE_REG_OFF_ARM64_X18, "bad offset for register x18");
static_assert(offsetof(FiberCtx, x19) == AXLE_REG_OFF_ARM64_X19, "bad offset for register x19");
static_assert(offsetof(FiberCtx, x20) == AXLE_REG_OFF_ARM64_X20, "bad offset for register x20");
static_assert(offsetof(FiberCtx, x21) == AXLE_REG_OFF_ARM64_X21, "bad offset for register x21");
static_assert(offsetof(FiberCtx, x22) == AXLE_REG_OFF_ARM64_X22, "bad offset for register x22");
static_assert(offsetof(FiberCtx, x23) == AXLE_REG_OFF_ARM64_X23, "bad offset for register x23");
static_assert(offsetof(FiberCtx, x24) == AXLE_REG_OFF_ARM64_X24, "bad offset for register x24");
static_assert(offsetof(FiberCtx, x25) == AXLE_REG_OFF_ARM64_X25, "bad offset for register x25");
static_assert(offsetof(FiberCtx, x26) == AXLE_REG_OFF_ARM64_X26, "bad offset for register x26");
static_assert(offsetof(FiberCtx, x27) == AXLE_REG_OFF_ARM64_X27, "bad offset for register x27");
static_assert(offsetof(FiberCtx, x28) == AXLE_REG_OFF_ARM64_X28, "bad offset for register x28");
static_assert(offsetof(FiberCtx, x29) == AXLE_REG_OFF_ARM64_X29, "bad offset for register x29");
static_assert(offsetof(FiberCtx,  d8) == AXLE_REG_OFF_ARM64_D8,  "bad offset for register d8");
static_assert(offsetof(FiberCtx,  d9) == AXLE_REG_OFF_ARM64_D9,  "bad offset for register d9");
static_assert(offsetof(FiberCtx, d10) == AXLE_REG_OFF_ARM64_D10, "bad offset for register d10");
static_assert(offsetof(FiberCtx, d11) == AXLE_REG_OFF_ARM64_D11, "bad offset for register d11");
static_assert(offsetof(FiberCtx, d12) == AXLE_REG_OFF_ARM64_D12, "bad offset for register d12");
static_assert(offsetof(FiberCtx, d13) == AXLE_REG_OFF_ARM64_D13, "bad offset for register d13");
static_assert(offsetof(FiberCtx, d14) == AXLE_REG_OFF_ARM64_D14, "bad offset for register d14");
static_assert(offsetof(FiberCtx, d15) == AXLE_REG_OFF_ARM64_D15, "bad offset for register d14");
static_assert(offsetof(FiberCtx,  sp) == AXLE_REG_OFF_ARM64_SP,  "bad offset for register sp");
static_assert(offsetof(FiberCtx,  lr) == AXLE_REG_OFF_ARM64_LR,  "bad offset for register lr");
// clang-format on

namespace axle {

void fiber_ctx_init(FiberCtx* ctx, std::span<uint8_t> stack, void (*target)(void*), void* arg);

} // namespace axle
