#include "axle/scheduler.h"

#include <csignal>

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "debug.h"
#include "fiber.h"
#include "axle/event.h"
#include "axle/status.h"

namespace axle {

thread_local Scheduler* Scheduler::k_instance = nullptr;
std::mutex Scheduler::k_schedulers_mtx;
std::unordered_map<std::thread::id, std::unique_ptr<Scheduler>> Scheduler::k_schedulers;

void Scheduler::enter(std::function<void()> entry_point) {
    Scheduler::init();
    k_instance->do_enter(std::move(entry_point));
    log_dbg("DONE");
}

void Scheduler::init() {
    if (k_instance == nullptr) {
        // Not using `std::make_unique` because `Scheduler`'s constructor is private.
        // We don't expect to frequent create instances of this class so separating the pointer's
        // control block from the data block is not an issue.
        const std::thread::id host_thread = std::this_thread::get_id();
        {
            const std::lock_guard<std::mutex> lk{k_schedulers_mtx};
            std::unique_ptr<Scheduler> scheduler(
                new Scheduler(std::make_shared<EventLoop>(), host_thread));
            (void)k_schedulers.emplace(host_thread, std::move(scheduler));
            k_instance = k_schedulers.at(host_thread).get();
        }
    }
}

void Scheduler::fini() {
    if (k_instance == nullptr) {
        return;
    }

    if (k_instance->host_thread_ != std::this_thread::get_id()) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lk{k_schedulers_mtx};
        k_instance->do_fini();
        (void)k_schedulers.erase(k_instance->host_thread_);
        k_instance = nullptr;
    }
}

void Scheduler::shutdown() {
    k_instance->do_shutdown();
}

void Scheduler::shutdown_all() {
    const std::lock_guard<std::mutex> lk{k_schedulers_mtx};
    for (auto&& [_, scheduler] : k_schedulers) {
        scheduler->do_shutdown();
    }
}

void Scheduler::schedule(std::function<void()> task, std::string tag) {
    k_instance->do_schedule(std::move(task), std::move(tag));
}

void Scheduler::run() {
    k_instance->do_run();
}

std::shared_ptr<Fiber> Scheduler::current_fiber() {
    return k_instance->current_fiber_;
}

void Scheduler::yield() {
    k_instance->do_yield();
}

void Scheduler::resume(std::shared_ptr<Fiber> fiber) {
    (void)k_instance->blocked_fibers_.erase(fiber);
    k_instance->enqueue(std::move(fiber));
}

std::shared_ptr<EventLoop> Scheduler::get_event_loop() {
    return k_instance->event_loop_;
}

Scheduler::Scheduler(std::shared_ptr<EventLoop> event_loop, std::thread::id host_thread)
    : main_fiber_{std::make_shared<Fiber>("main")},
      current_fiber_{main_fiber_},
      event_loop_{std::move(event_loop)},
      running_{false},
      host_thread_{host_thread} {}

void Scheduler::do_schedule(std::function<void()> task, std::string tag) {
    std::shared_ptr<Fiber> fiber = std::make_shared<Fiber>(
        [this, task = std::move(task)]() {
            task();
            Scheduler::terminate_fiber();
        },
        std::move(tag));
    log_dbg("SCHEDU {}", fiber);
    enqueue(std::move(fiber));
}

void Scheduler::do_yield() {
    if (!running_) {
        throw std::runtime_error("scheduler is not running");
    }
    log_dbg("YIELD  {}", Scheduler::current_fiber());
    (void)blocked_fibers_.insert(current_fiber_);
    switch_to(main_fiber_);
}

void Scheduler::do_fini() {
    log_dbg("FINI   {}", Scheduler::current_fiber());
    for (auto&& blocked_fiber : blocked_fibers_) {
        log_dbg("BLOCKD {}", blocked_fiber);
        enqueue(blocked_fiber);
    }
    blocked_fibers_.clear();

    // We need to interrupt fibers that became ready before the Scheduler was shutdown, but didn't
    // get a chance to run. Therefore, we enqueue all blocked fibers and then interrupt each fiber
    // just before resuming its execution.
    while (!ready_fibers_.empty()) {
        std::shared_ptr<Fiber> target = std::move(ready_fibers_.back());
        ready_fibers_.pop_back();
        log_dbg("INTRPT {}", target);
        target->interrupt();
        switch_to(std::move(target));
    }

    while (!terminated_fibers_.empty()) {
        terminated_fibers_.pop_back();
    }
}

void Scheduler::do_enter(std::function<void()> entry_point) {
    log_dbg("ENTER");
    do_schedule(std::move(entry_point), "entry_point");
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        throw std::runtime_error("could not replace signal handler for SIGINT");
    }
    const Status<None, int> status = event_loop_->register_signal(SIGINT, [&](auto status) {
        (void)status;
        do_schedule([&] { do_shutdown(); }, "shutdown");
    });

    if (status.is_err()) {
        throw std::runtime_error("could not register shutdown signal handler with event loop");
    }

    do_run();
    do_fini();
}

void Scheduler::do_shutdown() {
    // log_dbg("SHUTDN {}", Scheduler::current_fiber());
    running_.store(false);
}

void Scheduler::enqueue(std::shared_ptr<Fiber> fiber) {
    log_dbg("ENQUEU {}", fiber);
    ready_fibers_.push_back(std::move(fiber));
}

void Scheduler::do_run() {
    running_.store(true);

    while (running_) {
        while (!ready_fibers_.empty()) {
            std::shared_ptr<Fiber> target = std::move(ready_fibers_.back());
            ready_fibers_.pop_back();
            switch_to(std::move(target));
        }

        while (!terminated_fibers_.empty()) {
            terminated_fibers_.pop_back();
        }

        event_loop_->tick();
    }
}

void Scheduler::switch_to(std::shared_ptr<Fiber> target) {
    // We use a raw pointer instead of a `std::shared_ptr` to avoid leaking memory.
    // A `std::shared_ptr` will drop ref-count only if it goes out of scope, ...
    Fiber* source = current_fiber_.get();
    log_dbg("SWITCH {} -> {}", source, target);
    current_fiber_ = std::move(target);
    //  ... which is not guaranteed to happen because the next line might not return
    source->switch_to(current_fiber_.get());
}

void Scheduler::terminate_fiber() {
    log_dbg("TERMIN {}", current_fiber_);
    terminated_fibers_.push_back(current_fiber_);
    switch_to(main_fiber_);
}

} // namespace axle
