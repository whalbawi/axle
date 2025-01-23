#include "axle/socket.h"

#include <cerrno>
#include <cstdint>

#include <span>
#include <stdexcept>
#include <string>

#include "socket_common.h"
#include "axle/status.h"

namespace axle {

Socket::Socket() : Socket(socket::create_tcp()) {}

Socket::Socket(int fd) : fd_(fd) {
    if (fd_ == -1) {
        throw std::runtime_error("invalid socket fd");
    }

    if (socket::set_opt_nosigpipe(fd_).is_err()) {
        (void)close();

        throw std::runtime_error("could not set SO_NOSIGPIPE for socket");
    }
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket::~Socket() {
    (void)close();
}

Status<None, int> Socket::set_non_blocking() const {
    return socket::set_non_blocking(fd_);
}

Status<None, int> Socket::send_all(std::span<const uint8_t> buf) const {
    while (!buf.empty()) {
        Status<std::span<const uint8_t>, int> send_status = socket::send(fd_, buf);
        if (send_status.is_err()) {
            return Status<None, int>::make_err(send_status.err());
        }

        buf = send_status.ok();
    }

    return Status<None, int>::make_ok();
}

Status<std::span<uint8_t>, int> Socket::recv_some(std::span<uint8_t> buf) const {
    return socket::recv(fd_, buf);
}

Status<None, int> Socket::close() {
    Status<None, int> close_status = socket::close(fd_);
    if (close_status.is_err()) {
        return close_status;
    }

    fd_ = -1;

    return Status<None, int>::make_ok();
}

int Socket::get_fd() const {
    return fd_;
}

Status<None, int> ClientSocket::connect(const std::string& address, int port) const {
    return socket::connect(get_fd(), address, port);
}

ServerSocket::ServerSocket() {
    if (socket::set_opt_reuseaddr(get_fd()).is_err()) {
        throw std::runtime_error("could not set SO_REUSEADDR for socket");
    }
}

Status<None, int> ServerSocket::listen(int port, int backlog) const {
    return socket::listen(get_fd(), port, backlog);
}

Status<Socket, int> ServerSocket::accept() const {
    Status<int, int> accept_status = socket::accept(get_fd());
    if (accept_status.is_err()) {
        return Status<Socket, int>::make_err(accept_status.err());
    }

    return Status<Socket, int>::make_ok(Socket(accept_status.ok()));
}

} // namespace axle
