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

#if AXLE_TSAN_ENABLED

#include <sanitizer/tsan_interface.h>

#define AXLE_TSAN_CTX_DECLARE()                                                                    \
    struct TSanCtx {                                                                               \
        void* fiber;                                                                               \
        bool main_ctx;                                                                             \
    };                                                                                             \
    TSanCtx tsan_ctx_ {}

#define AXLE_TSAN_CTX_INIT(main_ctx__)                                                             \
    do {                                                                                           \
        tsan_ctx_.fiber = (main_ctx__) ? __tsan_get_current_fiber() : __tsan_create_fiber(0);      \
        tsan_ctx_.main_ctx = (main_ctx__);                                                         \
    } while (0)

#define AXLE_TSAN_CTX_FINI()                                                                       \
    if (!tsan_ctx_.main_ctx) {                                                                     \
        __tsan_destroy_fiber(tsan_ctx_.fiber);                                                     \
    }

#define AXLE_TSAN_SWITCH_TO_FIBER(from__, to__)                                                    \
    do {                                                                                           \
        (from__)->tsan_ctx_.fiber = __tsan_get_current_fiber();                                    \
        __tsan_switch_to_fiber((to__)->tsan_ctx_.fiber, 0);                                        \
    } while (0);

#else

#define AXLE_TSAN_CTX_DECLARE() static_assert(true) // NOLINT()
#define AXLE_TSAN_CTX_INIT(main_ctx__)
#define AXLE_TSAN_CTX_FINI()
#define AXLE_TSAN_SWITCH_TO_FIBER(from__, to__)

#endif // AXLE_TSAN_ENABLED
