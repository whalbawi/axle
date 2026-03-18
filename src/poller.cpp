#include "poller.h"

#include <time.h>
#include <unistd.h>
#include <sys/event.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include <array>
#include <stdexcept>
#include <vector>

namespace axle {

Poller::Poller() : poller_fd_(kqueue()) {
    if (poller_fd_ == -1) {
        throw std::runtime_error("failed to initialize poller with kqueue");
    }
}

std::vector<PollOutcome> Poller::poll() const {
    std::array<struct kevent, k_max_event_cnt> evs{};
    const struct timespec ts{.tv_sec = 0, .tv_nsec = 0};

    const int ret = kevent(poller_fd_, nullptr, 0, evs.data(), evs.size(), &ts);
    if (ret == -1) {
        perror("failed to wait for events");
        return {};
    }

    std::vector<PollOutcome> outcomes;
    outcomes.reserve(ret);

    for (int i = 0; i < ret; ++i) {
        const struct kevent& ev = evs.at(i);
        switch (ev.filter) {
        case EVFILT_USER: {
            (void)outcomes.emplace_back(ev.ident, PollEvent::USER, PollState::OK);
            break;
        }

        case EVFILT_READ: {
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            (void)outcomes.emplace_back(ev.ident, PollEvent::FD_READ, state);
            if ((ev.flags & EV_EOF) != 0) {
                (void)outcomes.emplace_back(ev.ident, PollEvent::FD_EOF, state);
            }
            break;
        }

        case EVFILT_WRITE: {
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            (void)outcomes.emplace_back(ev.ident, PollEvent::FD_WRITE, state);
            if ((ev.flags & EV_EOF) != 0) {
                (void)outcomes.emplace_back(ev.ident, PollEvent::FD_EOF, state);
            }
            break;
        }

        case EVFILT_TIMER: {
            // TODO (whalbawi): Confirm what happens when a timer fails and how errors are reported.
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            (void)outcomes.emplace_back(ev.ident, PollEvent::TIMER, state);
            break;
        }

        case EVFILT_SIGNAL: {
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            (void)outcomes.emplace_back(ev.ident, PollEvent::SIGNAL, state);
            break;
        }

        default:
            perror("unknown event type");
        }
    }

    return outcomes;
}

int Poller::register_user_event(int id) const {
    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_USER, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret != 0) {
        perror("failed to register kqueue user event");
        return errno;
    }

    return 0;
}

int Poller::register_fd_read(int fd) const {
    struct kevent ev{};
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register read filter for fd");

        return errno;
    };

    return 0;
}

int Poller::register_fd_write(int fd) const {
    struct kevent ev{};
    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register write filter for fd");

        return errno;
    };

    return 0;
}

int Poller::register_timer(uint64_t id, uint64_t timeout, bool periodic) const {
    struct kevent ev{};
    const uint16_t oneshot = periodic ? 0 : EV_ONESHOT;
    EV_SET(&ev, id, EVFILT_TIMER, EV_ADD | oneshot, NOTE_NSECONDS, timeout, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register timer");

        return errno;
    };

    return 0;
}

int Poller::register_signal(int signo) const {
    struct kevent ev{};
    EV_SET(&ev, signo, EVFILT_SIGNAL, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register signal filter");

        return errno;
    };

    return 0;
}

int Poller::notify_user(int id) const {
    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register signal filter");

        return errno;
    };

    return 0;
}

int Poller::remove_user_event(int id) const {
    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_USER, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove user filter");

        return errno;
    };

    return 0;
}

int Poller::remove_fd_read(int fd) const {
    struct kevent ev{};
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove read filter for fd");

        return errno;
    };

    return 0;
}

int Poller::remove_fd_write(int fd) const {
    struct kevent ev{};
    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove write filter for fd");

        return errno;
    };

    return 0;
}

int Poller::remove_timer(uint64_t id) const {
    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove timer filter");

        return errno;
    };

    return 0;
}

int Poller::remove_signal(int signo) const {
    struct kevent ev{};
    EV_SET(&ev, signo, EVFILT_SIGNAL, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove signal filter");

        return ret;
    };

    return 0;
}

Poller::~Poller() noexcept {
    const int ret = close(poller_fd_);
    if (ret == -1) {
        perror("failed to close poller file descriptor");
    }
}

} // namespace axle
