#pragma once

#include <cstddef>
#include <cstdint>

#include <cassert>
#include <unordered_map>
#include <unordered_set>
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

    int register_fd_read(int fd);
    int register_fd_write(int fd);
    int register_user_event();
    int register_timer(uint64_t timeout, bool periodic);
    int register_signal(int signo);

    int notify_user(int fd);

    int remove_fd_read(int fd);
    int remove_fd_write(int fd);
    int remove_user_event(int id);
    int remove_timer(int id);
    int remove_signal(int id);

  private:
    static constexpr size_t k_max_event_cnt = 64;

    int poller_fd_;
    std::unordered_map<int, uint32_t> fds_;
    std::unordered_set<int> eventfds_;
    std::unordered_set<int> timerfds_;
    std::unordered_map<int, int> signalfds_;
};

} // namespace axle
