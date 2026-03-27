#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdio>

#include <span>
#include <string>

#include "axle/status.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#define AXLE_USE_SO_NOSIGPIPE
#endif // MSG_NOSIGNAL

namespace axle::socket {

namespace detail {

inline Status<int, int> do_fcntl(int fd, int cmd, int flags) {

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const int ret = ::fcntl(fd, cmd, flags);
    if (ret == -1) {
        const int err = errno;
        perror("fcntl");

        return Err(err);
    }

    return Ok(ret);
}

inline struct sockaddr* endpoint_to_sockaddr(const std::string& address,
                                             int port,
                                             struct sockaddr_in& addr_in) {
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    addr_in.sin_addr.s_addr = inet_addr(address.c_str());

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<struct sockaddr*>(&addr_in);
}

template <typename T>
inline Status<T, int> do_getgetopt(const int fd, int opt) {
    T val = {};
    socklen_t val_sz = static_cast<socklen_t>(sizeof(val));

    if (::getsockopt(fd, SOL_SOCKET, opt, &val, &val_sz) == -1) {
        const int err = errno;
        perror("getsockopt");

        return Err(err);
    }

    return Ok(val);
}

inline Status<None, int> do_setsockopt(const int fd, int opt) {
    int enable = 1;
    if (::setsockopt(fd, SOL_SOCKET, opt, &enable, sizeof(enable)) == -1) {
        const int err = errno;
        perror("setsockopt");

        return Err(err);
    }

    return Ok();
}

} // namespace detail

inline Status<int, int> create_tcp() {
    const int ret = ::socket(AF_INET, SOCK_STREAM, 0);
    if (ret == -1) {
        const int err = errno;
        perror("socket");

        return Err(err);
    }

    return Ok(ret);
}

inline Status<size_t, int> get_sndbuf_sz(const int fd) {
    return detail::do_getgetopt<size_t>(fd, SO_SNDBUF);
}

inline Status<size_t, int> get_rcvbuf_sz(const int fd) {
    return detail::do_getgetopt<size_t>(fd, SO_RCVBUF);
}

inline Status<int, int> get_error(const int fd) {
    return detail::do_getgetopt<int>(fd, SO_ERROR);
}

inline Status<None, int> set_non_blocking(const int fd) {
    Status flags = detail::do_fcntl(fd, F_GETFL, 0);
    if (flags.is_err()) {
        return Err(flags.err());
    }

    Status status = detail::do_fcntl(fd, F_SETFL, flags.ok() | O_NONBLOCK);
    if (status.is_err()) {
        return Err(status.err());
    }

    return Ok();
}

inline Status<None, int> set_opt_nosigpipe(const int fd) {
#ifdef AXLE_USE_NOSIGPIPE
    return detail::do_setsockopt(fd, SO_NOSIGPIPE);
#else
    (void)fd;
    return Ok();
#endif // AXLE_USE_NOSIGPIPE
}

inline Status<None, int> set_opt_reuseaddr(const int fd) {
    return detail::do_setsockopt(fd, SO_REUSEADDR);
}

inline Status<std::span<const uint8_t>, int> send(const int fd,
                                                  const std::span<const uint8_t> buf) {
    const ssize_t len = ::send(fd, buf.data(), buf.size(), MSG_NOSIGNAL);
    if (len == -1) {
        const int err = errno;
        perror("send");

        return Err(err);
    }

    return Ok(buf.subspan(len));
}

inline Status<std::span<uint8_t>, int> recv(const int fd, const std::span<uint8_t> buf) {
    const ssize_t len = ::read(fd, buf.data(), buf.size());
    if (len == -1) {
        const int err = errno;
        perror("read");

        return Err(err);
    }

    return Ok(buf.first(len));
}

inline Status<None, int> listen(const int fd, const int port, const int backlog) {
    struct sockaddr_in addr_in{};
    struct sockaddr* addr = detail::endpoint_to_sockaddr("0.0.0.0", port, addr_in);
    if (::bind(fd, addr, sizeof(*addr)) == -1) {
        const int err = errno;
        perror("bind");

        return Err(err);
    }

    if (::listen(fd, backlog) < 0) {
        const int err = errno;
        perror("listen");

        return Err(err);
    }

    return Ok();
}

inline Status<int, int> accept(const int fd) {
    const int peer_fd = ::accept(fd, nullptr, nullptr);
    if (peer_fd == -1) {
        const int err = errno;
        perror("accept");

        return Err(err);
    }

    return Ok(peer_fd);
}

inline Status<None, int> connect(const int fd, const std::string& address, const int port) {
    struct sockaddr_in addr_in{};
    struct sockaddr* addr = detail::endpoint_to_sockaddr(address, port, addr_in);

    if (::connect(fd, addr, sizeof(*addr)) == -1) {
        const int err = errno;
        perror("connect");

        return Err(err);
    }

    return Ok();
}

inline Status<None, int> close(const int fd) {
    if (fd != -1 && ::close(fd) == -1) {
        const int err = errno;
        perror("close");

        return Err(err);
    }

    return Ok();
}

} // namespace axle::socket
