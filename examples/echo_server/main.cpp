#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <utility>

#include "axle/event.h"
#include "axle/tcp.h"

class Session {
  public:
    std::span<uint8_t> recv_buf(size_t max_len) {
        if (tail_ == buf_.end()) {
            return std::span<uint8_t>{};
        };

        const size_t len = std::min<size_t>(buf_.end() - tail_, max_len);

        return std::span<uint8_t>{tail_, tail_ + len};
    }

    void post_recv(std::span<uint8_t> buf) {
        std::advance(tail_, buf.size());
    }

    std::span<const uint8_t> send_buf(size_t max_len) {
        if (head_ == tail_) {
            return std::span<uint8_t>{};
        }

        const size_t len = std::min<size_t>(tail_ - head_, max_len);

        return std::span<const uint8_t>{head_, head_ + len};
    }

    void post_send(int64_t len) {
        std::advance(head_, len);
        if (tail_ == buf_.end() && head_ == tail_) {
            reset();
        }
    }

    void end() {}

    void reset() {
        head_ = buf_.begin();
        tail_ = buf_.begin();
    }

  private:
    static constexpr size_t k_buf_sz = 16 * 1024UL;

    std::array<uint8_t, k_buf_sz> buf_{};
    std::array<uint8_t, k_buf_sz>::iterator head_{buf_.begin()};
    std::array<uint8_t, k_buf_sz>::iterator tail_{buf_.begin()};
};

class EchoServer : public axle::TcpServer<Session> {
  public:
    explicit EchoServer(std::shared_ptr<axle::EventLoop> event_loop, int port)
        : TcpServer(std::move(event_loop), port) {}

    std::shared_ptr<Session> handle_connection() override {
        return std::make_shared<Session>();
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    constexpr int port = 8081;

    try {
        const std::shared_ptr<axle::EventLoop> event_loop = std::make_shared<axle::EventLoop>();
        EchoServer server{event_loop, port};

        server.start();
        event_loop->run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return -1;
    }
}
