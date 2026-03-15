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
    static constexpr size_t k_max_event_cnt = 64;

    int kq_;
    bool done_ = false;
    std::unordered_map<uint64_t, FdEventIOCb> fd_read_;
    std::unordered_map<uint64_t, FdEventIOCb> fd_write_;
    std::unordered_map<uint64_t, FdEventEOFCb> fd_eof_;
    std::unordered_map<uint64_t, TimerEventCb> timers_;
    std::unordered_map<uint64_t, std::pair<SignalEventCb, void (*)(int)>> signals_;

    void handle_fd_read(uint64_t fd, uint16_t flags, uint32_t fflags, int64_t data);
    void handle_fd_write(uint64_t fd, uint16_t flags, uint32_t fflags, int64_t data);
    void handle_timer(uint64_t id, uint16_t flags, uint32_t fflags);
    void handle_signal(uint64_t signo, uint16_t flags, uint32_t fflags, int64_t data);
    void handle_shutdown(uint64_t id);

    void do_shutdown();
};

} // namespace axle
