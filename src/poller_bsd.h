#pragma once

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <vector>

namespace axle {

struct PollOutcome;

class Poller {
  public:
    Poller();
    Poller(const Poller&) = delete;
    Poller(Poller&&) = delete;
    Poller& operator=(const Poller&) = delete;
    Poller& operator=(Poller&&) = delete;

    ~Poller() noexcept;

    std::vector<PollOutcome> poll() const;

    int register_fd_read(int fd) const;
    int register_fd_write(int fd) const;
    int register_user_event();
    int register_timer(uint64_t timeout, bool periodic);
    int register_signal(int signo) const;

    int notify_user(int id) const;

    int remove_fd_read(int fd) const;
    int remove_fd_write(int fd) const;
    int remove_user_event(int id) const;
    int remove_timer(int id) const;
    int remove_signal(int id) const;

  private:
    static constexpr size_t k_max_event_cnt = 64;

    int poller_fd_;
    std::atomic_int next_id_;
};

} // namespace axle
