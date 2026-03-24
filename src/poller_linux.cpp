#include "poller_linux.h"

#include <signal.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/types.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>

#include <array>
#include <span>
#include <stdexcept>
#include <vector>

#include "debug.h"
#include "poller_common.h"

namespace axle {

namespace {

int signal_procmask(int signo, int action) {
    // NOLINTNEXTLINE(misc-include-cleaner) -- https://github.com/llvm/llvm-project/issues/120830
    sigset_t signals{};
    if (sigemptyset(&signals) == -1) {
        const int err = errno;
        perror("could not create empty signal set");

        return err;
    }

    if (sigaddset(&signals, signo) == -1) {
        const int err = errno;
        perror("could not add signal to signal set");

        return err;
    }

    if (sigprocmask(action, &signals, nullptr) == -1) {
        const int err = errno;
        perror("could not modify signal mask");

        return err;
    }

    return 0;
}

int signal_block(int signo) {
    return signal_procmask(signo, SIG_BLOCK);
}

int signal_unblock(int signo) {
    return signal_procmask(signo, SIG_UNBLOCK);
}

PollState drain_fd(int fd, std::span<uint8_t> buf) {
    const ssize_t ret = read(fd, buf.data(), buf.size());
    if (ret == -1) {
        if (errno == EAGAIN) {
            return PollState::OK;
        }
        return PollState::ERR;
    }

    return (static_cast<size_t>(ret) == buf.size()) ? PollState::OK : PollState::ERR;
}

} // namespace

Poller::Poller() : poller_fd_(epoll_create1(0)) {
    if (poller_fd_ == -1) {
        throw std::runtime_error("failed to initialize epoll poller");
    }
}

Poller::~Poller() noexcept {
    const int ret = close(poller_fd_);
    if (ret == -1) {
        perror("failed to close poller file descriptor");
    }
}

std::vector<PollOutcome> Poller::poll() const {
    std::array<struct epoll_event, k_max_event_cnt> evs{};

    const int ret = epoll_wait(poller_fd_, evs.data(), evs.size(), 0);
    if (ret == -1) {
        perror("failed to wait for events");
        return {};
    }
    std::vector<PollOutcome> outcomes;
    outcomes.reserve(ret);

    for (int i = 0; i < ret; ++i) {
        const struct epoll_event& ev = evs.at(i);
        const uint32_t events = ev.events;
        const int fd = ev.data.fd;

        const PollState state = (events & EPOLLERR) != 0 ? PollState::ERR : PollState::OK;
        if ((events & EPOLLIN) != 0) {
            if (eventfds_.contains(fd)) {
                std::array<uint8_t, sizeof(uint64_t)> arr{};
                const PollState state = drain_fd(fd, std::span{arr});
                (void)outcomes.emplace_back(fd, PollEvent::USER, state);
                continue;
            }
            if (timerfds_.contains(fd)) {
                std::array<uint8_t, sizeof(uint64_t)> arr{};
                const PollState state = drain_fd(fd, std::span{arr});
                (void)outcomes.emplace_back(fd, PollEvent::TIMER, state);
                continue;
            }
            if (signalfds_.contains(fd)) {
                std::array<uint8_t, sizeof(struct signalfd_siginfo)> arr{};
                const PollState state = drain_fd(fd, std::span{arr});
                (void)outcomes.emplace_back(fd, PollEvent::SIGNAL, state);
                continue;
            }
            (void)outcomes.emplace_back(fd, PollEvent::FD_READ, state);
            if ((events & EPOLLRDHUP) != 0) {
                (void)outcomes.emplace_back(fd, PollEvent::FD_EOF, state);
            }
        }

        if ((events & EPOLLOUT) != 0) {
            (void)outcomes.emplace_back(fd, PollEvent::FD_WRITE, state);
            if ((events & EPOLLRDHUP) != 0) {
                (void)outcomes.emplace_back(ev.data.fd, PollEvent::FD_EOF, state);
            }
        }
    }

    return outcomes;
}

int Poller::register_fd_read(int fd) {
    uint32_t events = 0;
    int op = EPOLL_CTL_ADD;

    if (fds_.contains(fd)) {
        events = fds_.at(fd);
        op = EPOLL_CTL_MOD;
    }
    events |= EPOLLIN;

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;

    const int ret = epoll_ctl(poller_fd_, op, fd, &ev);
    if (ret == -1) {
        const int err = errno;
        perror("failed to register read epoll event for fd");

        return err;
    };

    fds_[fd] = events;

    return 0;
}

int Poller::register_fd_write(int fd) {
    uint32_t events = 0;
    int op = EPOLL_CTL_ADD;

    if (fds_.contains(fd)) {
        events = fds_.at(fd);
        op = EPOLL_CTL_MOD;
    }
    events |= EPOLLOUT;

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;

    const int ret = epoll_ctl(poller_fd_, op, fd, &ev);
    if (ret == -1) {
        const int err = errno;
        perror("failed to register write epoll event for fd");

        return err;
    };

    fds_[fd] = events;

    return 0;
}

int Poller::register_user_event() {
    const int fd = eventfd(0, EFD_NONBLOCK);
    if (fd < 0) {
        const int err = errno;
        perror("failed to create eventfd");

        return err;
    }

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN;

    const int ret = epoll_ctl(poller_fd_, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        const int err = errno;
        perror("failed to register epoll user event");

        return err;
    }

    const bool inserted = eventfds_.emplace(fd).second;
    AXLE_ASSERT(inserted);

    return fd;
}

int Poller::register_timer(uint64_t timeout, bool periodic) {
    // NOLINTNEXTLINE(misc-include-cleaner) -- https://github.com/llvm/llvm-project/issues/64336
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd == -1) {
        const int err = errno;
        perror("failed to create timerfd");

        return err;
    }

    // NOLINTNEXTLINE(misc-include-cleaner) -- https://github.com/llvm/llvm-project/issues/64336
    struct itimerspec ts = {};
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = static_cast<int64_t>(timeout);
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = periodic ? static_cast<int64_t>(timeout) : 0;

    const int ret = timerfd_settime(fd, 0, &ts, nullptr);
    if (ret == -1) {
        const int err = errno;
        perror("failed to set time on timerfd");

        return err;
    }

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN;

    const int epoll_ret = epoll_ctl(poller_fd_, EPOLL_CTL_ADD, fd, &ev);
    if (epoll_ret != 0) {
        const int err = errno;
        perror("failed to register timer event with epoll");

        return err;
    }

    const bool inserted = timerfds_.emplace(fd).second;
    AXLE_ASSERT(inserted);

    return fd;
}

int Poller::register_signal(int signo) {
    sigset_t signals;
    if (sigemptyset(&signals) == -1) {
        const int err = errno;
        perror("could not create empty signal set");

        return err;
    }

    if (sigaddset(&signals, signo) == -1) {
        const int err = errno;
        perror("could not add signal to signal set");

        return err;
    }

    const int fd = signalfd(-1, &signals, SFD_NONBLOCK);
    if (fd < 0) {
        const int err = errno;
        perror("failed to create signalfd");

        return err;
    }

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN;

    const int ret = epoll_ctl(poller_fd_, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        const int err = errno;
        perror("failed to register signal event with epoll");

        return err;
    }

    const int ret_signal = signal_block(signo);
    if (ret_signal != 0) {
        return ret_signal;
    }

    const bool inserted = signalfds_.insert_or_assign(fd, signo).second;
    AXLE_ASSERT(inserted);

    return fd;
}

int Poller::notify_user(int fd) {
    if (!eventfds_.contains(fd)) {
        return ENOENT;
    }

    uint64_t unused = 1;
    const ssize_t ret = write(fd, &unused, sizeof(unused));
    if (ret == -1) {
        const int err = errno;
        perror("could not write to eventfd for user notification");

        return err;
    }

    return 0;
}

int Poller::remove_fd_read(int fd) {
    uint32_t events = 0;

    if (fds_.contains(fd)) {
        events = fds_.at(fd);
    }
    events &= ~EPOLLIN;
    const int op = events == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;

    const int ret = epoll_ctl(poller_fd_, op, fd, &ev);
    if (ret == -1) {
        const int err = errno;
        perror("failed to fd read event from epoll");

        return err;
    };

    if (events == 0) {
        const size_t num_erased = fds_.erase(fd);
        AXLE_ASSERT(num_erased == 1);
    } else {
        fds_[fd] = events;
    }

    return 0;
}

int Poller::remove_fd_write(int fd) {
    uint32_t events = 0;

    if (fds_.contains(fd)) {
        events = fds_.at(fd);
    }
    events &= ~EPOLLOUT;
    const int op = events == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;

    const int ret = epoll_ctl(poller_fd_, op, fd, &ev);
    if (ret == -1) {
        const int err = errno;
        perror("failed to fd read event from epoll");

        return err;
    };

    if (events == 0) {
        const size_t num_erased = fds_.erase(fd);
        AXLE_ASSERT(num_erased == 1);
    } else {
        fds_[fd] = events;
    }

    return 0;
}

int Poller::remove_user_event(int id) {
    const size_t num_erased = eventfds_.erase(id);
    AXLE_ASSERT(num_erased == 1);

    const int ret = epoll_ctl(poller_fd_, EPOLL_CTL_DEL, id, nullptr);
    if (ret == -1) {
        const int err = errno;
        perror("failed to remove user event from epoll");

        return err;
    };

    (void)close(id);

    return 0;
}

int Poller::remove_timer(int id) {
    const size_t num_erased = timerfds_.erase(id);
    AXLE_ASSERT(num_erased == 1);

    const int ret = epoll_ctl(poller_fd_, EPOLL_CTL_DEL, id, nullptr);
    if (ret == -1) {
        const int err = errno;
        perror("failed to remove timer event from epoll");

        return err;
    };

    (void)close(id);

    return 0;
}

int Poller::remove_signal(int id) {
    const int signo = signalfds_.at(id);
    const size_t num_erased = signalfds_.erase(id);
    AXLE_ASSERT(num_erased == 1);

    const int ret_signal = signal_unblock(signo);
    if (ret_signal != 0) {
        return ret_signal;
    }

    const int ret = epoll_ctl(poller_fd_, EPOLL_CTL_DEL, id, nullptr);
    if (ret == -1) {
        const int err = errno;
        perror("failed to remove signal event from epoll");

        return err;
    };

    (void)close(id);

    return 0;
}

} // namespace axle
