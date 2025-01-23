#pragma once

#include <cstdint>

#include <span>
#include <string>

#include "axle/status.h"

namespace axle {

class Socket {
  public:
    Socket();
    explicit Socket(int fd);
    Socket& operator=(Socket&&) = delete;
    virtual ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;

    Status<None, int> set_non_blocking() const;

    Status<None, int> send_all(std::span<const uint8_t> buf_view) const;
    Status<std::span<uint8_t>, int> recv_some(std::span<uint8_t> buf_view) const;

    Status<None, int> close();

    int get_fd() const;

  private:
    int fd_;
};

class ClientSocket : public Socket {
  public:
    ClientSocket() = default;

    Status<None, int> connect(const std::string& address, int port) const;
};

class ServerSocket : public Socket {
  public:
    ServerSocket();

    Status<None, int> listen(int port, int backlog) const;
    Status<Socket, int> accept() const;
};

} // namespace axle
