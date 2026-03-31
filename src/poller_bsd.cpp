#include "poller_bsd.h"

#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/event.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>

#include <array>
#include <stdexcept>
#include <vector>

#include "common.h"
#include "poller_common.h"

namespace axle {

namespace {

int signal_block(int signo) {
    auto old = signal(signo, SIG_IGN);
    if (old == SIG_ERR) {
        return errno;
    }

    return 0;
}

int signal_unblock(int signo) {
    auto old = signal(signo, SIG_DFL);
    if (old == SIG_ERR) {
        return errno;
    }

    return 0;
}

} // namespace

Poller::Poller() : poller_fd_(kqueue()) {
    if (poller_fd_ == -1) {
        throw std::runtime_error("failed to initialize poller with kqueue");
    }
}

Poller::~Poller() noexcept {
    const int ret = close(poller_fd_);
    if (ret == -1) {
        perror("failed to close poller file descriptor");
    }
}

std::vector<PollOutcome> Poller::poll() const {
    std::array<struct kevent, k_max_event_cnt> evs{};

    const int ret = kevent(poller_fd_, nullptr, 0, evs.data(), evs.size(), nullptr);
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
            AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::USER, PollState::OK));
            break;
        }

        case EVFILT_READ: {
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::FD_READ, state));
            if ((ev.flags & EV_EOF) != 0) {
                AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::FD_EOF, state));
            }
            break;
        }

        case EVFILT_WRITE: {
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::FD_WRITE, state));
            if ((ev.flags & EV_EOF) != 0) {
                AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::FD_EOF, state));
            }
            break;
        }

        case EVFILT_TIMER: {
            // TODO (whalbawi): Confirm what happens when a timer fails and how errors are reported.
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::TIMER, state));
            break;
        }

        case EVFILT_SIGNAL: {
            const PollState state = (ev.flags & EV_ERROR) != 0 ? PollState::ERR : PollState::OK;
            AXLE_UNUSED(outcomes.emplace_back(ev.ident, PollEvent::SIGNAL, state));
            break;
        }

        default:
            perror("unknown event type");
        }
    }

    return outcomes;
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

int Poller::register_user_event() {
    const int id = next_id_++;
    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_USER, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret != 0) {
        perror("failed to register kqueue user event");
        return errno;
    }

    return id;
}

int Poller::register_timer(uint64_t timeout, bool periodic) {
    const int id = next_id_++;
    struct kevent ev{};
    const uint16_t oneshot = periodic ? 0 : EV_ONESHOT;
    EV_SET(&ev, id, EVFILT_TIMER, EV_ADD | oneshot, NOTE_NSECONDS, timeout, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register timer");

        return errno;
    };

    return id;
}

int Poller::register_signal(int signo) const {
    struct kevent ev{};
    EV_SET(&ev, signo, EVFILT_SIGNAL, EV_ADD, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to register signal filter");

        return errno;
    };

    const int ret_signal = signal_block(signo);
    if (ret_signal != 0) {
        return ret_signal;
    }

    return signo;
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

int Poller::remove_timer(int id) const {
    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove timer filter");

        return errno;
    };

    return 0;
}

int Poller::remove_signal(int id) const {
    const int ret_signal = signal_unblock(id);
    if (ret_signal != 0) {
        return ret_signal;
    }

    struct kevent ev{};
    EV_SET(&ev, id, EVFILT_SIGNAL, EV_DELETE, 0, 0, nullptr);

    const int ret = kevent(poller_fd_, &ev, 1, nullptr, 0, nullptr);
    if (ret == -1) {
        perror("failed to remove signal filter");

        return ret;
    };

    return 0;
}

} // namespace axle
