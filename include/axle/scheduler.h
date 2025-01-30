#pragma once

#include <functional>
#include <list>
#include <memory>

#include "fiber.h"
#include "axle/event.h"

namespace axle {

class Scheduler {
  public:
    static void init();
    static void terminate();

    static void schedule(std::function<void()> task);
    static void yield();
    static void resume(std::shared_ptr<Fiber> fiber);
    static std::shared_ptr<Fiber> current_fiber();
    static std::shared_ptr<EventLoop> get_event_loop();

  private:
    static std::unique_ptr<Scheduler> k_instance;

    explicit Scheduler(std::shared_ptr<EventLoop> event_loop);

    void do_schedule(std::function<void()> task);
    void do_yield();
    void terminate_fiber();
    void enqueue(std::shared_ptr<Fiber> fiber);
    void run();
    void switch_to(std::shared_ptr<Fiber> target);

    std::shared_ptr<Fiber> main_fiber_;
    std::shared_ptr<Fiber> loop_fiber_;
    std::shared_ptr<Fiber> current_fiber_;
    std::shared_ptr<EventLoop> event_loop_;

    std::list<std::shared_ptr<Fiber>> ready_fibers_;
    std::list<std::shared_ptr<Fiber>> terminated_fibers_;

    bool done_ = false;
};

} // namespace axle
