#pragma once

#include <concepts>
#include <format>
#include <memory>

#include "fiber.h"

#if AXLE_DEBUG_ENABLED
#include <iostream>
#include <thread>
#include <utility>

#include "axle/scheduler.h"
#endif

template <typename T>
concept pointer_to_fiber = requires(const T& t) {
    { std::to_address(t) } -> std::convertible_to<const axle::Fiber*>;
};

template <pointer_to_fiber T>
struct std::formatter<T> {
    static constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    static auto format(const T& fiber, std::format_context& ctx) {
        if (!fiber) {
            return std::format_to(ctx.out(), "{}", "(nil)");
        }
        if (fiber->tag_ == axle::Fiber::k_empty_tag) {
            return std::format_to(
                ctx.out(), "{:p}", static_cast<const void*>(std::to_address(fiber)));
        }

        return std::format_to(ctx.out(), "{}", fiber->tag_);
    }
};
namespace axle {

template <typename... Args>
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
void log_dbg(std::format_string<Args...> fmt, Args&&... args) {
    (void)fmt;
    ((void)args, ...);
#if AXLE_DEBUG_ENABLED
    constexpr size_t hash_mask = 0xffff;
    constexpr std::hash<std::thread::id> k_hasher;

    // NOLINTBEGIN
    std::cerr << std::format("[0x{:x}/{}] - ",
                             k_hasher(std::this_thread::get_id()) & hash_mask,
                             axle::Scheduler::current_fiber())
              // NOLINTEND
              << std::format(fmt, std::forward<Args>(args)...) << '\n';
#endif
}

} // namespace axle
