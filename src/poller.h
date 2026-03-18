#pragma once

#include <sys/event.h>

#include <vector>

namespace axle {

enum class PollState : uint8_t {
    ERR,
    OK,
};

enum class PollEvent : std::uint8_t {
    USER,
    FD_READ,
    FD_WRITE,
    FD_EOF,
    TIMER,
    SIGNAL,
};

struct PollOutcome {
    uint64_t id;
    PollEvent event;
    PollState state;
};

class Poller {
  public:
    Poller();
    Poller(const Poller&) = default;
    Poller(Poller&&) = delete;
    Poller& operator=(const Poller&) = default;
    Poller& operator=(Poller&&) = delete;

    ~Poller() noexcept;

    std::vector<PollOutcome> poll() const;

    int register_user_event(int id) const;
    int register_fd_read(int fd) const;
    int register_fd_write(int fd) const;
    int register_timer(uint64_t id, uint64_t timeout, bool periodic) const;
    int register_signal(int signo) const;

    int notify_user(int id) const;

    int remove_user_event(int id) const;
    int remove_fd_read(int fd) const;
    int remove_fd_write(int fd) const;
    int remove_timer(uint64_t id) const;
    int remove_signal(int signo) const;

  private:
    static constexpr size_t k_max_event_cnt = 64;

    int poller_fd_;
};

} // namespace axle
