#include "axle/event.h"

#include <unistd.h>
#include <sys/event.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <array>
#include <stdexcept>

#include "axle/status.h"

namespace axle {

EventLoop::EventLoop() : kq_(kqueue()) {
    if (kq_ == -1) {
        throw std::runtime_error("failed to initialize kqueue");
    }

    // Register shutdown handler
    struct kevent ev{};
    EV_SET(&ev, k_shutdown_event_id, EVFILT_USER, EV_ADD, 0, 0, nullptr);
    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        throw std::runtime_error("failed to register shutdown handler");
    }
}

EventLoop::~EventLoop() {
    const int ret = close(kq_);
    if (ret == -1) {
        perror("failed to close kqueue file descriptor");
    }
}

Status<None, int> EventLoop::register_fd_read(int fd, const FdEventIOCb& cb) {
    struct kevent ev{};

    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register read filter for fd");

        return Status<None, int>::make_err(errno);
    };
    fd_read_[fd] = cb;

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::register_fd_write(int fd, const FdEventIOCb& cb) {
    struct kevent ev{};

    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register write filter for fd");

        return Status<None, int>::make_err(errno);
    };
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
    struct kevent ev{};
    const uint16_t oneshot = periodic ? 0 : EV_ONESHOT;

    EV_SET(&ev, id, EVFILT_TIMER, EV_ADD | oneshot, NOTE_NSECONDS, timeout, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register timer");

        return Status<None, int>::make_err(errno);
    };
    timers_[id] = cb;

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::remove_fd_read(int fd) {
    if (fd_read_.erase(fd) != 1) {
        return Status<None, int>::make_err(0);
    }

    struct kevent ev{};

    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove read filter for fd");

        return Status<None, int>::make_err(errno);
    };

    return Status<None, int>::make_ok();
}

Status<None, int> EventLoop::remove_fd_write(int fd) {
    if (fd_write_.erase(fd) != 1) {
        return Status<None, int>::make_err(0);
    }

    struct kevent ev{};

    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove write filter for fd");

        return Status<None, int>::make_err(errno);
    };

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

    struct kevent ev{};

    EV_SET(&ev, id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove timer filter");

        return Status<None, int>::make_err(errno);
    };

    return Status<None, int>::make_ok();
}

void EventLoop::run() {

    std::array<struct kevent, k_max_event_cnt> evs{};

    while (!done_) {
        const int ret = kevent(kq_, nullptr, 0, evs.data(), evs.size(), nullptr);
        if (ret == -1) {
            perror("failed to wait for events");
            continue;
        }

        for (int i = 0; i < ret; ++i) {
            const struct kevent& ev = evs.at(i);
            switch (ev.filter) {
            case EVFILT_USER: {
                handle_shutdown(ev.ident);
                break;
            }

            case EVFILT_TIMER: {
                handle_timer(ev.ident, ev.flags, ev.data);
                break;
            }

            case EVFILT_READ: {
                handle_fd_read(ev.ident, ev.flags, ev.fflags, ev.data);
                break;
            }

            case EVFILT_WRITE: {
                handle_fd_write(ev.ident, ev.flags, ev.fflags, ev.data);
                break;
            }

            default:
                perror("unknown event type");
            }
        }
    }
}

Status<None, int> EventLoop::shutdown() const {
    struct kevent ev{};
    EV_SET(&ev, k_shutdown_event_id, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);

    const int ret = kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to schedule shutdown event");

        return Status<None, int>::make_err(errno);
    }

    return Status<None, int>::make_ok();
}

void EventLoop::handle_shutdown(const uint64_t id) {
    if (id == k_shutdown_event_id) {
        done_ = true;
    }
}

void EventLoop::handle_timer(const uint64_t id, const uint16_t flags, const uint32_t fflags) {
    if (!timers_.contains(id)) {
        return;
    }

    // TODO (whalbawi): Confirm what happens when a timer fails and how errors are reported.
    const TimerEventCb& cb = timers_[id];
    if ((flags & EV_ERROR) != 0) {
        cb(Status<None, uint32_t>::make_err(fflags));
    } else {
        cb(Status<None, uint32_t>::make_ok());
    }
}

void EventLoop::handle_fd_read(const uint64_t fd,
                               const uint16_t flags,
                               const uint32_t fflags,
                               const int64_t data) {
    if (fd_read_.contains(fd)) {
        const FdEventIOCb& cb = fd_read_[fd];
        if ((flags & EV_ERROR) != 0) {
            cb(Status<int64_t, uint32_t>::make_err(fflags));
        } else {
            cb(Status<int64_t, uint32_t>::make_ok(data));
        }
    }

    if ((flags & EV_EOF) != 0 && fd_eof_.contains(fd)) {
        const FdEventEOFCb& cb = fd_eof_[fd];
        cb();
    }
}

void EventLoop::handle_fd_write(const uint64_t fd,
                                const uint16_t flags,
                                const uint32_t fflags,
                                const int64_t data) {
    if (!fd_write_.contains(fd)) {
        return;
    }
    const FdEventIOCb& cb = fd_write_[fd];
    if ((flags & EV_ERROR) != 0) {
        cb(Status<int64_t, uint32_t>::make_err(fflags));
    } else {
        cb(Status<int64_t, uint32_t>::make_ok(data));
    }

    if ((flags & EV_EOF) != 0 && fd_eof_.contains(fd)) {
        const FdEventEOFCb& cb = fd_eof_[fd];
        cb();
    }
}

} // namespace axle
