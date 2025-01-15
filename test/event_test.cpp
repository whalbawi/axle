// NOLINTBEGIN(readability-function-cognitive-complexity)

#include "axle/event.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <array>
#include <atomic>
#include <span>
#include <string>
#include <thread>
#include <utility>

#include "axle/socket.h"
#include "axle/status.h"

#include "gtest/gtest.h"

namespace axle {

TEST(EventLoopTest, TimerOneshot) {
    EventLoop ev_loop;
    const int timer_id = 1;
    const uint64_t delay = 5e8;
    bool called = false;

    const TimerEventCb cb = [&](Status<None, uint32_t> status) {
        EXPECT_TRUE(status.is_ok());

        called = true;

        const Status<None, int> res = ev_loop.shutdown();
        EXPECT_TRUE(res.is_ok());
    };

    const Status<None, int> res = ev_loop.register_timer(timer_id, delay, false /* periodic */, cb);
    ASSERT_TRUE(res.is_ok());

    ev_loop.run();

    ASSERT_EQ(true, called);
}

TEST(EventLoopTest, TimerPeriodic) {
    EventLoop ev_loop;
    const int timer_id = 1;
    const uint64_t delay = 5e8;
    int counter = 0;
    int counter_max = 4;

    const TimerEventCb cb = [&](Status<None, uint32_t> status) {
        EXPECT_TRUE(status.is_ok());

        ++counter;

        if (counter == counter_max) {
            const Status<None, int> res = ev_loop.shutdown();
            EXPECT_TRUE(res.is_ok());
        }
    };

    const Status<None, int> res = ev_loop.register_timer(timer_id, delay, true /* periodic */, cb);
    ASSERT_TRUE(res.is_ok());

    ev_loop.run();

    ASSERT_EQ(counter_max, counter);
}

TEST(EventLoopTest, TimerUpdate) {
    EventLoop ev_loop;
    int timer_id = 1;
    const uint64_t delay = 5e8;
    int counter = 0;
    int counter_max = 4;
    bool called = false;

    const TimerEventCb cb_oneshot = [&](Status<None, uint32_t> status) {
        EXPECT_TRUE(status.is_ok());

        called = true;

        const Status<None, int> res = ev_loop.shutdown();
        EXPECT_TRUE(res.is_ok());
    };

    const TimerEventCb cb_periodic = [&](Status<None, uint32_t> status) {
        EXPECT_TRUE(status.is_ok());
        ++counter;
        if (counter == counter_max) {
            const Status<None, int> res =
                ev_loop.register_timer(timer_id, delay, false /* periodic */, cb_oneshot);
            EXPECT_TRUE(res.is_ok());
        }
    };

    const Status<None, int> res =
        ev_loop.register_timer(timer_id, delay, true /* periodic */, cb_periodic);
    ASSERT_TRUE(res.is_ok());

    ev_loop.run();

    ASSERT_EQ(counter_max, counter);
}

TEST(EventLoopTest, BadFd) {
    EventLoop ev_loop{};
    const int bogus_fd = 5;

    Status<None, int> status = ev_loop.register_fd_read(bogus_fd, [](Status<int64_t, uint32_t>) {});
    ASSERT_TRUE(status.is_err());
    ASSERT_EQ(EBADF, status.err());

    status = ev_loop.register_fd_write(bogus_fd, [](Status<int64_t, uint32_t>) {});
    ASSERT_TRUE(status.is_err());
    ASSERT_EQ(EBADF, status.err());

    ASSERT_TRUE(ev_loop.register_fd_eof(bogus_fd, []() {}).is_err());
}

class IncrementServer {
  public:
    IncrementServer() = delete;
    IncrementServer(const IncrementServer&) = delete;
    IncrementServer& operator=(const IncrementServer&) = delete;
    IncrementServer(IncrementServer&&) = delete;
    IncrementServer& operator=(IncrementServer&&) = delete;

    explicit IncrementServer(int port) : port_(port), socket_{ServerSocket()}, running_{false} {}

    ~IncrementServer() {
        (void)socket_.close();
    }

    void start() {
        if (socket_.listen(port_, 1).is_err()) {
            return;
        }

        running_.store(true);

        while (running_.load()) {
            Status<Socket, int> peer_socket_status = socket_.accept();
            if (peer_socket_status.is_err()) {
                continue;
            }

            Socket peer_socket = peer_socket_status.ok();
            for (;;) {
                std::span<uint8_t> buf_view{buf_};
                Status<std::span<uint8_t>, int> recv_some_res = peer_socket.recv_some(buf_view);
                if (recv_some_res.is_err()) {
                    (void)peer_socket.close();
                    break;
                }

                buf_view = recv_some_res.ok();

                if (buf_view.empty()) {
                    (void)peer_socket.close();
                    break;
                }

                for (auto& elem : buf_view) {
                    elem += 1;
                }

                if (peer_socket.send_all(buf_view).is_err()) {
                    (void)peer_socket.close();
                    break;
                }
            }
        }
    }

    bool running() {
        return running_.load();
    }

    void stop() {
        running_.store(false);
    }

  private:
    static constexpr size_t k_buf_sz = 16;

    int port_;
    ServerSocket socket_;
    std::atomic_bool running_;
    std::array<uint8_t, k_buf_sz> buf_{};
};

class Request {
  public:
    enum class State : uint8_t {
        WRITE,
        READ,
        END,
    };

    explicit Request(Socket&& socket,
                     std::span<uint8_t> send_buf_view,
                     std::span<uint8_t> recv_buf_view)
        : socket_(std::move(socket)),
          send_buf_view_{send_buf_view},
          recv_buf_view_{recv_buf_view} {}

    void write(size_t max_len) {
        const size_t len = std::min<size_t>(send_buf_view_.size(), max_len);
        const Status<None, int> res = socket_.send_all(send_buf_view_.first(len));
        if (res.is_err()) {
            return;
        }

        send_buf_view_ = send_buf_view_.subspan(len);
        if (send_buf_view_.empty()) {
            state_ = State::READ;
        }
    }

    void read(size_t max_len) {
        const size_t len = std::min<size_t>(recv_buf_view_.size(), max_len);
        Status<std::span<uint8_t>, int> res = socket_.recv_some(recv_buf_view_.first(len));
        if (res.is_err()) {
            return;
        }

        recv_buf_view_ = recv_buf_view_.subspan(res.ok().size());
        if (recv_buf_view_.empty()) {
            state_ = State::END;
        }
    }

    void end() {
        (void)socket_.close();
    }

    State get_state() {
        return state_;
    }

  private:
    Socket socket_;
    std::span<uint8_t> send_buf_view_;
    std::span<uint8_t> recv_buf_view_;

    State state_ = State::WRITE;
};

TEST(EventLoopTest, Server) {
    const int port = 8080;
    IncrementServer server{port};

    std::thread server_thread{[&] { server.start(); }};
    while (!server.running()) {
    }

    ClientSocket socket{};
    ASSERT_TRUE(socket.connect("127.0.0.1", port).is_ok());
    ASSERT_TRUE(socket.set_non_blocking().is_ok());
    constexpr size_t buf_sz = 512;

    std::array<uint8_t, buf_sz> send_buf{};
    for (size_t i = 0; i < send_buf.size(); ++i) {
        send_buf.at(i) = 'a' + i;
    }

    std::array<uint8_t, buf_sz> recv_buf{};

    const int conn_fd = socket.get_fd();
    Request request{std::move(socket), std::span<uint8_t>{send_buf}, std::span<uint8_t>{recv_buf}};

    EventLoop ev_loop{};
    const FdEventEOFCb eof_cb = [&]() { FAIL(); };

    const FdEventIOCb read_cb = [&](Status<int64_t, uint32_t> status) {
        EXPECT_TRUE(status.is_ok());

        switch (request.get_state()) {
        case Request::State::READ: {
            request.read(status.ok());
            break;
        }
        default: {
            return;
        }
        }
    };

    const FdEventIOCb write_cb = [&](Status<int64_t, uint32_t> status) {
        EXPECT_TRUE(status.is_ok());

        switch (request.get_state()) {
        case Request::State::WRITE: {
            request.write(status.ok());
            break;
        }
        case Request::State::END: {
            server.stop();
            request.end();
            ASSERT_TRUE(ev_loop.shutdown().is_ok());
            break;
        }
        default: {
            return;
        }
        }
    };

    ASSERT_TRUE(ev_loop.register_fd_write(conn_fd, write_cb).is_ok());
    ASSERT_TRUE(ev_loop.register_fd_read(conn_fd, read_cb).is_ok());
    ASSERT_TRUE(ev_loop.register_fd_eof(conn_fd, eof_cb).is_ok());

    ev_loop.run();
    server_thread.join();

    for (size_t i = 0; i < recv_buf.size(); ++i) {
        const uint8_t received = recv_buf.at(i);
        const uint8_t expected = send_buf.at(i) + 1;
        ASSERT_EQ(expected, received);
    }
}

} // namespace axle
// NOLINTEND(readability-function-cognitive-complexity)
