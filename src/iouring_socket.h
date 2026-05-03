#include <memory>
#include <span>

#include "axle/gate.h"

namespace axle {

class IOUring;

class CompletionAsyncSocket {
  public:
    CompletionAsyncSocket();
    explicit CompletionAsyncSocket(int fd);
    virtual ~CompletionAsyncSocket();

    CompletionAsyncSocket(CompletionAsyncSocket&& other) noexcept;
    CompletionAsyncSocket(const CompletionAsyncSocket&) = delete;
    CompletionAsyncSocket& operator=(const CompletionAsyncSocket&) = delete;
    CompletionAsyncSocket& operator=(CompletionAsyncSocket&&) = delete;

    Status<None, int> send_all(std::span<const uint8_t> buf_view) const;
    Status<std::span<uint8_t>, int> recv_some(std::span<uint8_t> buf_view) const;
    Status<None, int> close();

    int get_fd() const;

  protected:
    std::shared_ptr<IOUring> io_uring_; // NOLINT(misc-non-private-member-variables-in-classes)
    Gate gate_;                         // NOLINT(misc-non-private-member-variables-in-classes)

  private:
    int fd_;
};

class AsyncClientSocket : public CompletionAsyncSocket {
  public:
    AsyncClientSocket() = default;

    Status<None, int> connect(const std::string& address, int port) const;
};

class AsyncServerSocket : public CompletionAsyncSocket {
  public:
    AsyncServerSocket();

    Status<None, int> listen(int port, int backlog) const;
    Status<CompletionAsyncSocket, int> accept() const;
};

} // namespace axle
