#include "fiber.h"
#include "axle/event.h"
#include "axle/socket.h"

namespace axle {

class AsyncSocket {
  public:
    AsyncSocket();
    explicit AsyncSocket(int fd);
    AsyncSocket(AsyncSocket&& other) noexcept;
    virtual ~AsyncSocket();

    AsyncSocket(const AsyncSocket&) = delete;
    AsyncSocket& operator=(const AsyncSocket&) = delete;
    AsyncSocket& operator=(AsyncSocket&&) = delete;

    Status<None, int> send_all(std::span<const uint8_t> buf_view) const;
    Status<std::span<uint8_t>, int> recv_some(std::span<uint8_t> buf_view) const;
    Status<None, int> close();

    int get_fd() const;

  protected:
    std::shared_ptr<EventLoop> event_loop_; // NOLINT(misc-non-private-member-variables-in-classes)

  private:
    int fd_;
};

class AsyncClientSocket : public AsyncSocket {
  public:
    AsyncClientSocket() = default;

    Status<None, int> connect(const std::string& address, int port) const;
};

class AsyncServerSocket : public AsyncSocket {
  public:
    AsyncServerSocket();

    Status<None, int> listen(int port, int backlog) const;
    Status<AsyncSocket, int> accept() const;
};

} // namespace axle
