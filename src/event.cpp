#include "axle/event.h"

#include <poller.h>
#include <time.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "axle/status.h"

namespace axle {

EventLoop::EventLoop() : poller_(std::make_unique<Poller>()) {
    // Register shutdown handler
    if (poller_->register_user_event(k_shutdown_event_id) != 0) {
        throw std::runtime_error("failed to register shutdown handler");
    }
}

EventLoop::~EventLoop() = default;

Status<None, int> EventLoop::register_fd_read(int fd, const FdEventIOCb& cb) {
    int ret = poller_->register_fd_read(fd);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    fd_read_[fd] = cb;

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::register_fd_write(int fd, const FdEventIOCb& cb) {
    int ret = poller_->register_fd_write(fd);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    fd_write_[fd] = cb;

    return Status<None, int>::make_ok();
}

Status<None, None> EventLoop::register_fd_eof(int fd, const FdEventEOFCb& cb) {
    if (!fd_read_.contains(fd) && !fd_write_.contains(fd)) {
        return Status<None, None>::make_err();
    }

    fd_eof_[fd] = cb;

    return Status<None, None>::make_ok();
}

Status<None, int> EventLoop::register_timer(uint64_t id,
                                            uint64_t timeout,
                                            bool periodic,
                                            const TimerEventCb& cb) {
    int ret = poller_->register_timer(id, timeout, periodic);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    timers_[id] = cb;

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::register_signal(int signo, const SignalEventCb& cb) {
    // Save the old handler so that we can restore it when `remove_signal` is called.
    void (*const old_handler)(int) = signal(signo, SIG_IGN);

    if (old_handler == SIG_ERR) {
        perror("could not clear signal handler");

        return Status<None, int>::make_err(errno);
    }

    int ret = poller_->register_signal(signo);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    signals_[signo] = std::make_pair(cb, old_handler);

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::remove_fd_read(int fd) {
    if (fd_read_.erase(fd) != 1) {
        return Status<None, int>::make_err(0);
    }

    int ret = poller_->remove_fd_read(fd);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::remove_fd_write(int fd) {
    if (fd_write_.erase(fd) != 1) {
        return Status<None, int>::make_err(0);
    }

    int ret = poller_->remove_fd_write(fd);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    return Status<None, int>::make_ok();
}

Status<None, None> EventLoop::remove_fd_eof(int fd) {
    if (fd_eof_.erase(fd) != 1) {
        return Status<None, None>::make_err();
    }

    return Status<None, None>::make_ok();
}

Status<None, int> EventLoop::remove_timer(uint64_t id) {
    if (timers_.erase(id) != 1) {
        return Status<None, int>::make_err(0);
    }

    int ret = poller_->remove_timer(id);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::remove_signal(int signo) {
    void (*sighandler)(int) = signal(signo, signals_.at(signo).second);

    if (sighandler == SIG_ERR) {
        perror("failed to reset signal handler");
        return Status<None, int>::make_err(0);
    }

    int ret = poller_->remove_signal(signo);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    if (signals_.erase(signo) != 1) {
        return Status<None, int>::make_err(0);
    }

    return Status<None, int>::make_ok();
}

void EventLoop::tick() {
    const std::vector<PollOutcome> outcomes = poller_->poll();

    for (auto&& outcome : outcomes) {
        switch (outcome.event) {

        case PollEvent::USER: {
            handle_shutdown(outcome);
            break;
        }

        case PollEvent::FD_READ: {
            handle_fd_read(outcome);
            break;
        }
        case PollEvent::FD_WRITE: {
            handle_fd_write(outcome);
            break;
        }
        case PollEvent::FD_EOF: {
            handle_fd_eof(outcome);
            break;
        }
        case PollEvent::TIMER: {
            handle_timer(outcome);
            break;
        }
        case PollEvent::SIGNAL:
            handle_signal(outcome);
            break;
        }
    }
}

void EventLoop::run() {
    while (!done_) {
        tick();
    }
}

Status<None, int> EventLoop::shutdown() const {
    int ret = poller_->notify_user(k_shutdown_event_id);
    if (ret != 0) {
        perror("failed to schedule shutdown event");

        return Status<None, int>::make_err(ret);
    }

    return Status<None, int>::make_ok();
}

void EventLoop::handle_fd_read(const PollOutcome& outcome) {
    if (!fd_read_.contains(outcome.id)) {
        return;
    }
    const FdEventIOCb& cb = fd_read_[outcome.id];
    if (outcome.state == PollState::ERR) {
        cb(Status<int64_t, uint32_t>::make_err(0));
    } else {
        cb(Status<int64_t, uint32_t>::make_ok(0));
    }
}

void EventLoop::handle_fd_write(const PollOutcome& outcome) {
    if (!fd_write_.contains(outcome.id)) {
        return;
    }

    const FdEventIOCb& cb = fd_write_[outcome.id];
    if (outcome.state == PollState::ERR) {
        cb(Status<int64_t, uint32_t>::make_err(0));
    } else {
        cb(Status<int64_t, uint32_t>::make_ok(0));
    }
}

void EventLoop::handle_fd_eof(const PollOutcome& outcome) {
    if (!fd_eof_.contains(outcome.id)) {
        return;
    }

    const FdEventEOFCb& cb = fd_eof_[outcome.id];
    cb();
}

void EventLoop::handle_timer(const PollOutcome& outcome) {
    if (!timers_.contains(outcome.id)) {
        return;
    }

    const TimerEventCb& cb = timers_[outcome.id];
    if (outcome.state == PollState::ERR) {
        cb(Status<None, uint32_t>::make_err(0));
    } else {
        cb(Status<None, uint32_t>::make_ok());
    }
}

void EventLoop::handle_signal(const PollOutcome& outcome) {
    if (!signals_.contains(outcome.id)) {
        return;
    }

    const SignalEventCb& cb = signals_[outcome.id].first;
    if (outcome.state == PollState::ERR) {
        cb(Status<int64_t, uint32_t>::make_err(0));
    } else {
        cb(Status<int64_t, uint32_t>::make_ok(0));
    }
}

void EventLoop::handle_shutdown(const PollOutcome& outcome) {
    if (outcome.id == k_shutdown_event_id) {
        done_ = true;
    }
}

} // namespace axle
