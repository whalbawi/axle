#pragma once

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "axle/event.h"

namespace axle {

class Fiber;

class Scheduler {
  public:
    static void init();
    static void fini();
    static void shutdown();
    static void shutdown_all();

    static void schedule(std::function<void()> task);

    static std::shared_ptr<Fiber> current_fiber();
    static void yield();
    static void resume(std::shared_ptr<Fiber> fiber);

    static std::shared_ptr<EventLoop> get_event_loop();

    ~Scheduler() = default;

  private:
    thread_local static Scheduler* k_instance;

    static std::mutex k_schedulers_mtx;
    static std::unordered_map<std::thread::id, std::unique_ptr<Scheduler>> k_schedulers;

    explicit Scheduler(std::shared_ptr<EventLoop> event_loop, std::thread::id host_thread);
    Scheduler() = delete;
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    void do_fini();
    void do_shutdown();
    void do_schedule(std::function<void()> task);
    void do_yield();

    void enqueue(std::shared_ptr<Fiber> fiber);
    void run();
    void switch_to(std::shared_ptr<Fiber> target);
    void terminate_fiber();

    std::shared_ptr<Fiber> main_fiber_;
    std::shared_ptr<Fiber> loop_fiber_;
    std::shared_ptr<Fiber> current_fiber_;
    std::shared_ptr<EventLoop> event_loop_;

    std::list<std::shared_ptr<Fiber>> ready_fibers_;
    std::unordered_set<std::shared_ptr<Fiber>> blocked_fibers_;
    std::list<std::shared_ptr<Fiber>> terminated_fibers_;

    std::atomic_bool running_;
    const std::thread::id host_thread_;
};

} // namespace axle
