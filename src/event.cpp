#include "axle/event.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include <memory>
#include <stdexcept>
#include <vector>

#include "poller.h"
#include "axle/status.h"

namespace axle {

EventLoop::EventLoop() : poller_(std::make_unique<Poller>()) {
    // Register shutdown handler
    const int fd = poller_->register_user_event();
    if (fd == -1) {
        throw std::runtime_error("failed to register shutdown handler");
    }

    shutdown_event_id_ = fd;
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

Status<int, int> EventLoop::register_timer(uint64_t timeout,
                                           bool periodic,
                                           const TimerEventCb& cb) {
    int fd = poller_->register_timer(timeout, periodic);
    if (fd == -1) {
        return Status<int, int>::make_err(fd);
    }

    timers_[fd] = cb;

    return Status<int, int>::make_ok(fd);
}

Status<int, int> EventLoop::register_signal(int signo, const SignalEventCb& cb) {
    int fd = poller_->register_signal(signo);
    if (fd == -1) {
        return Status<int, int>::make_err(fd);
    }

    signals_[fd] = cb;

    return Status<int, int>::make_ok(fd);
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

Status<None, int> EventLoop::remove_timer(int id) {
    if (timers_.erase(id) != 1) {
        return Status<None, int>::make_err(0);
    }

    int ret = poller_->remove_timer(id);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::remove_signal(int fd) {
    int ret = poller_->remove_signal(fd);
    if (ret != 0) {
        return Status<None, int>::make_err(ret);
    }

    if (signals_.erase(fd) != 1) {
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
    int ret = poller_->notify_user(shutdown_event_id_);
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

    const TimerEventCb cb = timers_[outcome.id];
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

    const SignalEventCb& cb = signals_[outcome.id];
    if (outcome.state == PollState::ERR) {
        cb(Status<int64_t, uint32_t>::make_err(0));
    } else {
        cb(Status<int64_t, uint32_t>::make_ok(0));
    }
}

void EventLoop::handle_shutdown(const PollOutcome& outcome) {
    if (outcome.id == shutdown_event_id_) {
        done_ = true;
    }
}

} // namespace axle
