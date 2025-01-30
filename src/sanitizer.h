#pragma once

#if AXLE_ASAN_ENABLED

#include <sanitizer/common_interface_defs.h>

#define AXLE_ASAN_CTX_DECLARE()                                                                    \
    struct ASanCtx {                                                                               \
        Fiber* from;                                                                               \
        void* stack_base;                                                                          \
        size_t stack_sz;                                                                           \
    };                                                                                             \
    ASanCtx asan_ctx_ {}

#define AXLE_ASAN_CTX_INIT(stack_base__, stack_sz__)                                               \
    do {                                                                                           \
        asan_ctx_.stack_base = (void*)(stack_base__);                                              \
        asan_ctx_.stack_sz = (stack_sz__);                                                         \
    } while (0)

#define AXLE_ASAN_START_SWITCH_FIBER(from__, to__)                                                 \
    do {                                                                                           \
        ASanCtx* asan_ctx__ = &(to__)->asan_ctx_;                                                  \
        asan_ctx__->from = from__;                                                                 \
        __sanitizer_start_switch_fiber(nullptr, asan_ctx__->stack_base, asan_ctx__->stack_sz);     \
    } while (0)

#define AXLE_ASAN_FINISH_SWITCH_FIBER(to__)                                                        \
    do {                                                                                           \
        ASanCtx* asan_ctx__ = &(to__)->asan_ctx_.from->asan_ctx_;                                  \
        __sanitizer_finish_switch_fiber(                                                           \
            nullptr, (const void**)&asan_ctx__->stack_base, &asan_ctx__->stack_sz);                \
    } while (0)

#else

#define AXLE_ASAN_CTX_DECLARE() static_assert(true) // NOLINT()
#define AXLE_ASAN_CTX_INIT(stack_base__, stack_sz__)
#define AXLE_ASAN_START_SWITCH_FIBER(from__, to__)
#define AXLE_ASAN_FINISH_SWITCH_FIBER(to__)

#endif // AXLE_ASAN_ENABLED
