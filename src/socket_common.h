#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>

static int do_fcntl(int fd, int cmd, int flags) {
    return fcntl(fd, cmd, flags); // NOLINT(cppcoreguidelines-pro-type-vararg, misc-include-cleaner)
}

static struct sockaddr* endpoint_to_sockaddr(const std::string& address,
                                             int port,
                                             struct sockaddr_in& addr_in) {
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port); // NOLINT(misc-include-cleaner)
    addr_in.sin_addr.s_addr = inet_addr(address.c_str());

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<struct sockaddr*>(&addr_in);
}

static axle::Status<axle::None, int> do_setsockopt(const int fd, int opt) {
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, opt, &enable, sizeof(enable)) == -1) {
        return axle::Status<axle::None, int>::make_err(errno);
    }

    return axle::Status<axle::None, int>::make_ok();
}

namespace axle::socket {

inline int create_tcp() {
    return ::socket(AF_INET, SOCK_STREAM, 0);
}

inline Status<None, int> set_non_blocking(const int fd) {
    const int flags = do_fcntl(fd, F_GETFL, 0); // NOLINT(misc-include-cleaner) -- for F_GETFL
    if (flags == -1) {
        return Status<None, int>::make_err(errno);
    }

    // NOLINTNEXTLINE(misc-include-cleaner) -- for F_SETFL, O_NONBLOCK
    if (do_fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return Status<None, int>::make_err(errno);
    }

    return Status<None, int>::make_ok();
}

inline Status<None, int> set_opt_nosigpipe(const int fd) {
    return do_setsockopt(fd, SO_NOSIGPIPE);
}

inline Status<None, int> set_opt_reuseaddr(const int fd) {
    return do_setsockopt(fd, SO_REUSEADDR);
}

inline Status<std::span<const uint8_t>, int> send(const int fd,
                                                  const std::span<const uint8_t> buf) {
    // NOLINTNEXTLINE(misc-include-cleaner) -- for ssize_t
    const ssize_t len = write(fd, buf.data(), buf.size());
    if (len == -1) {
        return Status<std::span<const uint8_t>, int>::make_err(errno);
    }

    return Status<std::span<const uint8_t>, int>::make_ok(buf.subspan(len));
}

inline Status<std::span<uint8_t>, int> recv(const int fd, const std::span<uint8_t> buf) {
    const ssize_t len = read(fd, buf.data(), buf.size());
    if (len == -1) {
        return Status<std::span<uint8_t>, int>::make_err(errno);
    }

    return Status<std::span<uint8_t>, int>::make_ok(buf.first(len));
}

inline Status<None, int> listen(const int fd, const int port, const int backlog) {
    struct sockaddr_in addr_in{};
    struct sockaddr* addr = endpoint_to_sockaddr("0.0.0.0", port, addr_in);
    if (bind(fd, addr, sizeof(*addr)) == -1) {
        return Status<None, int>::make_err(errno);
    }

    if (::listen(fd, backlog) < 0) {
        return Status<None, int>::make_err(errno);
    }

    return Status<None, int>::make_ok();
}

inline Status<int, int> accept(const int fd) {
    const int peer_fd = ::accept(fd, nullptr, nullptr);
    if (peer_fd == -1) {
        return Status<int, int>::make_err(errno);
    }

    return Status<int, int>::make_ok(peer_fd);
}

inline Status<None, int> connect(const int fd, const std::string& address, const int port) {
    struct sockaddr_in addr_in{};
    struct sockaddr* addr = endpoint_to_sockaddr(address, port, addr_in);

    if (::connect(fd, addr, sizeof(*addr)) == -1) {
        return Status<None, int>::make_err(errno);
    }

    return Status<None, int>::make_ok();
}

inline Status<None, int> close(const int fd) {
    if (fd != -1 && ::close(fd) == -1) {
        return Status<None, int>::make_err(errno);
    }

    return Status<None, int>::make_ok();
}

} // namespace axle::socket
