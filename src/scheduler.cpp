#include "axle/scheduler.h"

#include <functional>
#include <memory>
#include <utility>

#include "fiber.h"
#include "axle/event.h"

namespace axle {

std::unique_ptr<Scheduler> Scheduler::k_instance = nullptr;

void Scheduler::init() {
    if (k_instance == nullptr) {
        // Not using `std::make_unique` because `Scheduler`'s constructor is private.
        // We don't expect to frequent create instances of this class so separating the pointer's
        // control block from the data block is not an issue.
        k_instance = std::unique_ptr<Scheduler>(new Scheduler(std::make_shared<EventLoop>()));
    }
}

void Scheduler::terminate() {
    k_instance->done_ = true;
}

void Scheduler::schedule(std::function<void()> task) {
    k_instance->do_schedule(std::move(task));
}

void Scheduler::yield() {
    k_instance->do_yield();
}

void Scheduler::resume(std::shared_ptr<Fiber> fiber) {
    k_instance->enqueue(std::move(fiber));
}

std::shared_ptr<Fiber> Scheduler::current_fiber() {
    return k_instance->current_fiber_;
}

std::shared_ptr<EventLoop> Scheduler::get_event_loop() {
    return k_instance->event_loop_;
}

Scheduler::Scheduler(std::shared_ptr<EventLoop> event_loop)
    : main_fiber_{std::make_shared<Fiber>()},
      loop_fiber_{std::make_shared<Fiber>([] { k_instance->run(); })},
      current_fiber_{main_fiber_},
      event_loop_{std::move(event_loop)} {}

void Scheduler::do_schedule(std::function<void()> task) {
    std::shared_ptr<Fiber> fiber = std::make_shared<Fiber>([this, task = std::move(task)]() {
        task();
        Scheduler::terminate_fiber();
    });
    enqueue(std::move(fiber));
}

void Scheduler::do_yield() {
    switch_to(loop_fiber_);
}

void Scheduler::terminate_fiber() {
    terminated_fibers_.push_back(current_fiber_);
    do_yield();
}

void Scheduler::enqueue(std::shared_ptr<Fiber> fiber) {
    ready_fibers_.push_back(std::move(fiber));
}

void Scheduler::run() {
    for (;;) {
        while (!ready_fibers_.empty()) {
            std::shared_ptr<Fiber> target = std::move(ready_fibers_.back());
            ready_fibers_.pop_back();
            switch_to(std::move(target));
        }

        while (!terminated_fibers_.empty()) {
            terminated_fibers_.pop_back();
        }

        if (done_) {
            switch_to(main_fiber_);
        }

        event_loop_->tick();
    }
}

void Scheduler::switch_to(std::shared_ptr<Fiber> target) {
    // We use a raw pointer instead of a `std::shared_ptr` to avoid leaking memory.
    // A `std::shared_ptr` will drop ref-count only if it goes out of scope, ...
    Fiber* source = current_fiber_.get();
    current_fiber_ = std::move(target);
    // ... which is not guaranteed to happen becuase the next line might not return
    source->switch_to(current_fiber_.get());
}

} // namespace axle
