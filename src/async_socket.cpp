#include "axle/async_socket.h"

#include <cerrno>
#include <cstdint>

#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "common.h"
#include "debug.h"
#include "fiber.h" // IWYU pragma: keep
#include "socket_common.h"
#include "axle/event.h"
#include "axle/gate.h"
#include "axle/scheduler.h"
#include "axle/status.h"

namespace axle {

AsyncSocket::AsyncSocket()
    : AsyncSocket([] {
          Status fd_status = socket::create_tcp();
          if (fd_status.is_err()) {
              throw std::runtime_error{"could not create TCP socket"};
          }

          return fd_status.ok();
      }()) {}

AsyncSocket::AsyncSocket(int fd) : event_loop_(Scheduler::get_event_loop()), fd_(fd) {
    if (socket::set_non_blocking(fd_).is_err()) {
        const Status close_st = close();
        AXLE_ASSERT(close_st.is_ok());

        throw std::runtime_error("failed to put socket in non-blocking mode");
    }

    if (socket::set_opt_nosigpipe(fd_).is_err()) {
        const Status close_st = close();
        AXLE_ASSERT(close_st.is_ok());

        throw std::runtime_error("could not set SO_NOSIGPIPE for socket");
    }

    const Status<None, int> ev_status =
        event_loop_->register_fd_read(get_fd(), [gate = gate_](Status<int64_t, uint32_t> status) {
            // TODO(whalbawi, 310326): Handle properly
            AXLE_UNUSED(status);
            gate.post();
        });

    if (ev_status.is_err()) {
        throw std::runtime_error("could not register fd with event loop for read readiness");
    }
}

AsyncSocket::AsyncSocket(AsyncSocket&& other) noexcept
    : event_loop_(std::move(other.event_loop_)), gate_(std::move(other.gate_)), fd_(other.fd_) {
    other.fd_ = -1;
}

AsyncSocket::~AsyncSocket() {
    const Status close_st = close();
    AXLE_ASSERT(close_st.is_ok());
}

Status<None, int> AsyncSocket::send_all(std::span<const uint8_t> buf_view) const {
    while (!buf_view.empty()) {
        Status<std::span<const uint8_t>, int> send_status = socket::send(fd_, buf_view);
        if (send_status.is_ok()) {
            buf_view = send_status.ok();
            continue;
        }

        const int err = send_status.err();
        switch (err) {
        case EWOULDBLOCK: {
            Gate gate{};
            Status<None, int> ev_status =
                event_loop_->register_fd_write(get_fd(), [&gate](Status<int64_t, uint32_t> status) {
                    AXLE_ASSERT(status.is_ok());
                    gate.post();
                });
            if (ev_status.is_err()) {
                return Err(ev_status.err());
            }
            const Status wait_st = gate.wait();
            const Status remove_st = event_loop_->remove_fd_write(get_fd());
            AXLE_ASSERT(remove_st.is_ok());
            if (wait_st.is_err()) {
                return Err(EINTR);
            }
            continue;
        }
        default:
            return Err(err);
        }
    }

    return Ok();
}

Status<std::span<uint8_t>, int> AsyncSocket::recv_some(std::span<uint8_t> buf_view) const {
    for (;;) {
        Status<std::span<uint8_t>, int> recv_status = socket::recv(fd_, buf_view);
        if (recv_status.is_ok()) {
            return recv_status;
        }

        const int err = recv_status.err();
        switch (err) {
        case EWOULDBLOCK: {
            const Status wait_st = gate_.wait();
            if (wait_st.is_err()) {
                const Status rem = event_loop_->remove_fd_read(get_fd());
                AXLE_ASSERT(rem.is_ok());
                return Err(EINTR);
            }
            continue;
        }
        default:
            return Err(err);
        }
    }
}

Status<None, int> AsyncSocket::close() {
    if (event_loop_) {
        const Status rem_write_st = event_loop_->remove_fd_write(get_fd());
        AXLE_UNUSED(rem_write_st.is_ok());
        const Status rem_read_st = event_loop_->remove_fd_read(get_fd());
        AXLE_UNUSED(rem_read_st.is_ok());
    }

    Status<None, int> close_status = socket::close(fd_);
    if (close_status.is_err()) {
        return close_status;
    }

    fd_ = -1;

    return Ok();
}

int AsyncSocket::get_fd() const {
    return fd_;
}

Status<None, int> AsyncClientSocket::connect(const std::string& address, int port) const {
    for (;;) {
        Status<None, int> connect_status = socket::connect(get_fd(), address, port);
        if (connect_status.is_ok()) {
            return Ok();
        }

        const int err = connect_status.err();
        switch (err) {
        case EINPROGRESS: {
            const Gate gate{};
            Status<None, int> ev_status =
                event_loop_->register_fd_write(get_fd(), [gate](Status<int64_t, uint32_t> status) {
                    AXLE_UNUSED(status.is_ok());
                    gate.post();
                });
            if (ev_status.is_err()) {
                return Err(ev_status.err());
            }

            const Status wait_st = gate.wait();
            const Status rem_st = event_loop_->remove_fd_write(get_fd());
            AXLE_ASSERT(rem_st.is_ok());
            if (wait_st.is_err()) {
                return Err(EINTR);
            }

            Status sockopt = socket::get_error(get_fd());
            if (sockopt.is_err()) {
                return Err(sockopt.err());
            }

            const int sockerr = sockopt.ok();
            if (sockerr != 0) {
                return Err(sockerr);
            }

            return Ok();
        }

        default:
            return Err(err);
        }
    }
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
            return Ok(AsyncSocket(accept_status.ok()));
        }

        const int err = accept_status.err();
        switch (err) {
        case EWOULDBLOCK: {
            const Status wait_st = gate_.wait();
            if (wait_st.is_err()) {
                const Status rem_st = event_loop_->remove_fd_read(get_fd());
                AXLE_ASSERT(rem_st.is_ok());
                return Err(EINTR);
            }
            continue;
        }
        default:
            return Err(err);
        }
    }
}

} // namespace axle
