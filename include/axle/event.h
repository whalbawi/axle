#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <unordered_map>

#include "axle/status.h"

namespace axle {

using FdEventIOCb = std::function<void(Status<int64_t, uint32_t>)>;
using FdEventEOFCb = std::function<void()>;
using TimerEventCb = std::function<void(Status<None, uint32_t>)>;
using SignalEventCb = std::function<void(Status<int64_t, uint32_t>)>;

class Poller;
struct PollOutcome;

class EventLoop {
  public:
    EventLoop();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    ~EventLoop();

    Status<None, int> register_fd_read(int fd, const FdEventIOCb& cb);
    Status<None, int> register_fd_write(int fd, const FdEventIOCb& cb);
    Status<None, None> register_fd_eof(int fd, const FdEventEOFCb& cb);
    Status<None, int> register_timer(uint64_t id,
                                     uint64_t timeout,
                                     bool periodic,
                                     const TimerEventCb& cb);
    Status<None, int> register_signal(int signo, const SignalEventCb& cb);

    Status<None, int> remove_fd_read(int fd);
    Status<None, int> remove_fd_write(int fd);
    Status<None, None> remove_fd_eof(int fd);
    Status<None, int> remove_timer(uint64_t id);
    Status<None, int> remove_signal(int signo);

    void tick();
    void run();

    Status<None, int> shutdown() const;

  private:
    static constexpr uint64_t k_shutdown_event_id = 19;

    std::unique_ptr<Poller> poller_;
    bool done_ = false;
    std::unordered_map<uint64_t, FdEventIOCb> fd_read_;
    std::unordered_map<uint64_t, FdEventIOCb> fd_write_;
    std::unordered_map<uint64_t, FdEventEOFCb> fd_eof_;
    std::unordered_map<uint64_t, TimerEventCb> timers_;
    std::unordered_map<uint64_t, std::pair<SignalEventCb, void (*)(int)>> signals_;

    void handle_fd_read(const PollOutcome& outcome);
    void handle_fd_write(const PollOutcome& outcome);
    void handle_fd_eof(const PollOutcome& outcome);
    void handle_timer(const PollOutcome& outcome);
    void handle_signal(const PollOutcome& outcome);
    void handle_shutdown(const PollOutcome& outcome);

    void do_shutdown();
};

} // namespace axle
