#pragma once

#include <cstdint>

#include <span>

#include "fiber_reg_off_x64.h"

struct FiberCtx {
    uintptr_t rbx;
    uintptr_t rbp;
    uintptr_t rdi;
    uintptr_t rsi;
    uintptr_t rsp;

    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;

    uintptr_t rip;
};

// clang-format off
static_assert(offsetof(FiberCtx, rbx) == AXLE_REG_OFF_X64_RBX, "bad offset for register rbx");
static_assert(offsetof(FiberCtx, rdi) == AXLE_REG_OFF_X64_RDI, "bad offset for register rdi");
static_assert(offsetof(FiberCtx, rsi) == AXLE_REG_OFF_X64_RSI, "bad offset for register rsi");
static_assert(offsetof(FiberCtx, rsp) == AXLE_REG_OFF_X64_RSP, "bad offset for register rsp");
static_assert(offsetof(FiberCtx, r12) == AXLE_REG_OFF_X64_R12, "bad offset for register r12");
static_assert(offsetof(FiberCtx, r13) == AXLE_REG_OFF_X64_R13, "bad offset for register r13");
static_assert(offsetof(FiberCtx, r14) == AXLE_REG_OFF_X64_R14, "bad offset for register r14");
static_assert(offsetof(FiberCtx, r15) == AXLE_REG_OFF_X64_R15, "bad offset for register r15");
static_assert(offsetof(FiberCtx, rsp) == AXLE_REG_OFF_X64_RSP, "bad offset for register rsp");
// clang-format on

namespace axle {

void fiber_ctx_init(FiberCtx* ctx, std::span<uint8_t> stack, void (*target)(void*), void* arg);

} // namespace axle
