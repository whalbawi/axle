#include "axle/async_socket.h"

#include <cerrno>
#include <cstdint>

#include <span>
#include <stdexcept>
#include <utility>

#include "socket_common.h"
#include "axle/event.h"
#include "axle/scheduler.h"
#include "axle/status.h"

namespace axle {

AsyncSocket::AsyncSocket() : AsyncSocket(socket::create_tcp()) {}

AsyncSocket::AsyncSocket(int fd) : event_loop_(Scheduler::get_event_loop()), fd_(fd) {
    if (fd_ == -1) {
        throw std::runtime_error("invalid socket fd");
    }

    if (socket::set_non_blocking(fd_).is_err()) {
        (void)close();

        throw std::runtime_error("failed to put socket in non-blocking mode");
    }

    if (socket::set_opt_nosigpipe(fd_).is_err()) {
        (void)close();

        throw std::runtime_error("could not set SO_NOSIGPIPE for socket");
    }
}

AsyncSocket::AsyncSocket(AsyncSocket&& other) noexcept
    : event_loop_(std::move(other.event_loop_)), fd_(other.fd_) {
    other.fd_ = -1;
}

AsyncSocket::~AsyncSocket() {
    (void)close();
}

Status<None, int> AsyncSocket::send_all(std::span<const uint8_t> buf_view) const {
    while (!buf_view.empty()) {
        Status<std::span<const uint8_t>, int> send_status = socket::send(fd_, buf_view);
        if (send_status.is_ok()) {
            buf_view = send_status.ok();
            continue;
        }

        int err = send_status.err();
        switch (err) {
        case EWOULDBLOCK: {
            Status<None, int> ev_status = event_loop_->register_fd_write(
                get_fd(), [fiber = Scheduler::current_fiber()](Status<int64_t, uint32_t> status) {
                    if (status.is_err() && status.err() == EINTR) {
                        fiber->interrupt();
                    }
                    Scheduler::resume(fiber);
                });
            if (ev_status.is_err()) {
                return Status<None, int>::make_err(ev_status.err());
            }
            Scheduler::yield();
            if (Scheduler::current_fiber()->interrupted()) {
                return Status<None, int>::make_err(EINTR);
            }
            continue;
        }
        default:
            return Status<None, int>::make_err(err);
        }
    }

    return Status<None, int>::make_ok();
}

Status<std::span<uint8_t>, int> AsyncSocket::recv_some(std::span<uint8_t> buf_view) const {
    for (;;) {
        Status<std::span<uint8_t>, int> recv_status = socket::recv(fd_, buf_view);
        if (recv_status.is_ok()) {
            return recv_status;
        }

        int err = recv_status.err();
        switch (err) {
        case EWOULDBLOCK: {
            Status<None, int> ev_status = event_loop_->register_fd_read(
                get_fd(), [fiber = Scheduler::current_fiber()](Status<int64_t, uint32_t> status) {
                    if (status.is_err() && status.err() == EINTR) {
                        fiber->interrupt();
                    }
                    Scheduler::resume(fiber);
                });
            if (ev_status.is_err()) {
                return Status<std::span<uint8_t>, int>::make_err(ev_status.err());
            }
            Scheduler::yield();
            if (Scheduler::current_fiber()->interrupted()) {
                return Status<std::span<uint8_t>, int>::make_err(EINTR);
            }
            continue;
        }
        default:
            return Status<std::span<uint8_t>, int>::make_err(err);
        }
    }
}

Status<None, int> AsyncSocket::close() {
    if (event_loop_) {
        (void)event_loop_->remove_fd_write(get_fd());
        (void)event_loop_->remove_fd_read(get_fd());
    }

    Status<None, int> close_status = socket::close(fd_);
    if (close_status.is_err()) {
        return close_status;
    }

    fd_ = -1;

    return Status<None, int>::make_ok();
}

int AsyncSocket::get_fd() const {
    return fd_;
}

AsyncServerSocket::AsyncServerSocket() {
    if (socket::set_opt_reuseaddr(get_fd()).is_err()) {
        throw std::runtime_error("could not set SO_REUSEADDR for socket");
    }
}

Status<None, int> AsyncServerSocket::listen(int port, int backlog) const {
    return socket::listen(get_fd(), port, backlog);
}

Status<AsyncSocket, int> AsyncServerSocket::accept() const {
    for (;;) {
        Status<int, int> accept_status = socket::accept(get_fd());
        if (accept_status.is_ok()) {
            return Status<AsyncSocket, int>::make_ok(AsyncSocket(accept_status.ok()));
        }

        int err = accept_status.err();
        switch (err) {
        case EWOULDBLOCK: {
            Status<None, int> ev_status = event_loop_->register_fd_read(
                get_fd(), [fiber = Scheduler::current_fiber()](Status<int64_t, uint32_t> status) {
                    if (status.is_err() && status.err() == EINTR) {
                        fiber->interrupt();
                    }
                    Scheduler::resume(fiber);
                });
            if (ev_status.is_err()) {
                return Status<AsyncSocket, int>::make_err(ev_status.err());
            }
            Scheduler::yield();
            if (Scheduler::current_fiber()->interrupted()) {
                return Status<AsyncSocket, int>::make_err(EINTR);
            }
            continue;
        }
        default:
            return Status<AsyncSocket, int>::make_err(err);
        }
    }
}

} // namespace axle
