// NOLINTBEGIN(readability-function-cognitive-complexity)

#include "axle/async_socket.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "socket_common.h"
#include "axle/gate.h"
#include "axle/scheduler.h"
#include "axle/socket.h"
#include "axle/status.h"

#include "gtest/gtest.h"

#include "test_common.h"

namespace axle {

namespace {

constexpr int k_test_port_base = 9000;
constexpr int k_listen_backlog = 8;
constexpr size_t k_buf_sz = 4096;

int test_port() {
    static std::atomic<int> counter{0};
    return k_test_port_base + counter.fetch_add(1);
}

class Server {
  public:
    explicit Server(int port) : port_(port) {}
    Server(const Server&) = delete;
    Server(const Server&&) = delete;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&) = delete;

    void start() {
        thread_ = std::thread([this] { run(); });
        {
            std::unique_lock lk{mtx_};
            cv_ready_.wait(lk, [this] { return ready_; });
        }
    }

    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    virtual ~Server() {
        join();
    }

  private:
    virtual void connection_handler(Socket client) = 0;

    void run() {
        const axle::ServerSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.listen(port_, k_listen_backlog));
        {
            const std::lock_guard lk{mtx_};
            ready_ = true;
            cv_ready_.notify_one();
        }

        Status<Socket, int> accept = socket.accept();
        AXLE_TEST_ASSERT_OK(accept);
        Socket client = accept.ok();

        connection_handler(std::move(client));
    }

    int port_;
    std::thread thread_;

    std::mutex mtx_;
    bool ready_ = false;
    std::condition_variable cv_ready_;
};

class EchoServer : public Server {
  public:
    explicit EchoServer(int port) : Server(port) {}

  private:
    void connection_handler(Socket client) override {
        for (;;) {
            std::array<uint8_t, k_buf_sz> buf{};
            Status<std::span<uint8_t>, int> recv = client.recv_some(std::span(buf));
            ASSERT_TRUE(recv.is_ok());
            const std::span<uint8_t> recv_buf = recv.ok();

            if (recv_buf.empty()) {
                ASSERT_TRUE(client.close().is_ok());
                break;
            }

            const Status<None, int> send = client.send_all(recv_buf);
            ASSERT_TRUE(send.is_ok());
        }
    }
};

class RecvServer : public Server {
  public:
    RecvServer(int port, std::span<uint8_t> payload) : Server(port), payload_(payload) {}

  private:
    virtual void before_recv() {};
    virtual void before_close() {};

    void connection_handler(Socket client) override {
        before_recv();
        for (std::span<uint8_t> view = payload_; !view.empty();) {
            Status<std::span<uint8_t>, int> recv = client.recv_some(view);
            AXLE_TEST_ASSERT_OK(recv);
            const std::span<uint8_t> recv_view = recv.ok();

            if (recv_view.empty()) {
                break;
            }

            view = view.subspan(recv_view.size());
        }

        before_close();
        AXLE_TEST_ASSERT_OK(client.close());
    }

    std::span<uint8_t> payload_;
};

class RecvOnSignalServer : public RecvServer {
  public:
    RecvOnSignalServer(int port, std::span<uint8_t> payload) : RecvServer(port, payload) {}

    void signal_recv() {
        {
            const std::lock_guard<std::mutex> lk{mtx_};
            signaled_recv_ = true;
            cv_recv_.notify_one();
        }
    }

    void signal_close() {
        {
            const std::lock_guard<std::mutex> lk{mtx_};
            signaled_close_ = true;
            cv_close_.notify_one();
        }
    }

  private:
    void before_recv() override {
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_recv_.wait(lk, [&] { return signaled_recv_.load(); });
        }
    }

    void before_close() override {
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_close_.wait(lk, [&] { return signaled_close_.load(); });
        }
    }

    std::mutex mtx_;
    std::condition_variable cv_recv_;
    std::condition_variable cv_close_;
    std::atomic_bool signaled_recv_;
    std::atomic_bool signaled_close_;
};

class SendServer : public Server {
  public:
    SendServer(int port, std::span<uint8_t> payload) : Server(port), payload_(payload) {}

  private:
    virtual void before_send() {};
    virtual void before_close() {};

    void connection_handler(Socket client) override {
        before_send();
        AXLE_TEST_ASSERT_OK(client.send_all(payload_));
        before_close();
        AXLE_TEST_ASSERT_OK(client.close());
    }

    std::span<uint8_t> payload_;
};

class SendOnSignalServer : public SendServer {
  public:
    SendOnSignalServer(int port, std::span<uint8_t> payload) : SendServer(port, payload) {}

    void signal_send() {
        {
            const std::lock_guard<std::mutex> lk{mtx_};
            signaled_send_ = true;
            cv_send_.notify_one();
        }
    }

    void signal_close() {
        {
            const std::lock_guard<std::mutex> lk{mtx_};
            signaled_close_ = true;
            cv_close_.notify_one();
        }
    }

  private:
    void before_send() override {
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_send_.wait(lk, [&] { return signaled_send_.load(); });
        }
    }

    void before_close() override {
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_close_.wait(lk, [&] { return signaled_close_.load(); });
        }
    }

    std::mutex mtx_;
    std::condition_variable cv_send_;
    std::condition_variable cv_close_;
    std::atomic_bool signaled_send_;
    std::atomic_bool signaled_close_;
};

class ResetServer : public Server {
  public:
    explicit ResetServer(int port) : Server(port) {}

  private:
    virtual void before_close() {}

    void connection_handler(Socket client) override {
        before_close();
        AXLE_TEST_ASSERT_OK(client.close());
    }
};

class ResetOnSignalServer : public ResetServer {
  public:
    explicit ResetOnSignalServer(int port) : ResetServer(port) {}

    void signal_close() {
        {
            const std::lock_guard<std::mutex> lk{mtx_};
            signaled_close_ = true;
            cv_close_.notify_one();
        }
    }

  private:
    void before_close() override {
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_close_.wait(lk, [&] { return signaled_close_.load(); });
        }
    }

    std::mutex mtx_;
    std::condition_variable cv_close_;
    std::atomic_bool signaled_close_;
};

} // namespace

TEST(AsyncSocketTest, SendRecvEcho) {
    const int port = test_port();
    EchoServer server{port};

    server.start();
    Scheduler::enter([&] {
        const AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));
        std::array<uint8_t, k_buf_sz> send_buf{};
        rnd_buf(send_buf);

        AXLE_TEST_ASSERT_OK(socket.send_all(send_buf));

        std::array<uint8_t, k_buf_sz> recv_buf{};
        for (std::span<uint8_t> view{recv_buf}; !view.empty();) {
            Status<std::span<uint8_t>, int> recv = socket.recv_some(view);
            AXLE_TEST_ASSERT_OK(recv);
            const std::span<uint8_t> recv_view = recv.ok();
            if (recv_view.empty()) {
                break;
            }

            view = view.subspan(recv_view.size());
        }

        ASSERT_EQ(recv_buf, send_buf);
        Scheduler::shutdown();
    });
    server.join();
}

TEST(AsyncSocketTest, SendAllBackpressure) {
    Status fd_status = socket::create_tcp();
    AXLE_TEST_ASSERT_OK(fd_status);

    const int fd = fd_status.ok();

    // Send a large buffer so that `send_all` blocks
    Status<size_t, int> sndbuf_sz_status = socket::get_sndbuf_sz(fd);
    AXLE_TEST_ASSERT_OK(sndbuf_sz_status);
    const size_t send_buf_sz = sndbuf_sz_status.ok();

    Status<size_t, int> rcvbuf_sz_status = socket::get_rcvbuf_sz(fd);
    AXLE_TEST_ASSERT_OK(rcvbuf_sz_status);
    const size_t recv_buf_sz = rcvbuf_sz_status.ok();

    const size_t buf_sz = 2 * (recv_buf_sz + send_buf_sz);

    const int port = test_port();
    std::vector<uint8_t> payload(buf_sz);

    RecvServer server{port, payload};
    server.start();

    Scheduler::enter([fd, port, buf_sz, &payload, &server] {
        AXLE_TEST_ASSERT_OK(socket::connect(fd, "127.0.0.1", port));
        AsyncSocket socket{fd};

        std::vector<uint8_t> buf;
        buf.resize(buf_sz);
        rnd_buf(buf);
        AXLE_TEST_ASSERT_OK(socket.send_all(buf));

        std::array<uint8_t, 1> eof{};
        Status recv = socket.recv_some(eof);
        AXLE_TEST_ASSERT_OK(recv);

        server.join();
        ASSERT_EQ(payload, buf);
        ASSERT_TRUE(recv.ok().empty());

        AXLE_TEST_ASSERT_OK(socket.close());

        Scheduler::shutdown();
    });
}

TEST(AsyncClientSocketTest, ConnectSuccess) {
    const int port = test_port();
    ResetServer server{port};

    server.start();
    Scheduler::enter([port] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        std::array<uint8_t, 1> buf{};
        const std::span<uint8_t> buf_view{buf};
        Status<std::span<uint8_t>, int> recv = socket.recv_some(buf_view);
        AXLE_TEST_ASSERT_OK(recv);
        ASSERT_TRUE(recv.ok().empty());

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

TEST(AsyncClientSocketTest, ConnectRefused) {
    Scheduler::enter([] {
        AsyncClientSocket socket{};
        ASSERT_TRUE(socket.connect("127.0.0.1", 1234).is_err());
        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
}

TEST(AsyncClientSocketTest, ServerNonBlockingSendAndClose) {
    const int port = test_port();
    std::array<uint8_t, k_buf_sz> payload{};
    rnd_buf(payload);
    SendOnSignalServer server{port, payload};
    server.start();
    // Have the server send the payload and close the socket before the call to `recv_some` below.
    server.signal_send();
    server.signal_close();

    Scheduler::enter([port, &payload] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        // Receive payload
        std::array<uint8_t, k_buf_sz> recv_buf{};
        for (std::span<uint8_t> view{recv_buf}; !view.empty();) {
            Status<std::span<uint8_t>, int> recv = socket.recv_some(view);
            AXLE_TEST_ASSERT_OK(recv);
            const std::span<uint8_t> recv_view = recv.ok();
            if (recv_view.empty()) {
                break;
            }

            view = view.subspan(recv_view.size());
        }
        ASSERT_EQ(payload, recv_buf);

        // Receive EOF
        const std::span<uint8_t> buf_view{recv_buf};
        Status<std::span<uint8_t>, int> recv = socket.recv_some(buf_view);
        AXLE_TEST_ASSERT_OK(recv);
        ASSERT_TRUE(recv.ok().empty());

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

TEST(AsyncClientSocketTest, ServerBlockingSendAndClose) {
    const int port = test_port();
    std::array<uint8_t, k_buf_sz> payload{};
    rnd_buf(payload);
    SendOnSignalServer server{port, payload};
    server.start();

    Scheduler::enter([port, &server, &payload] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        // Runs after the first call to `recv_some` below blocks.
        Scheduler::schedule([&server] {
            server.signal_send();
            server.signal_close();
        });

        // Receive payload
        std::array<uint8_t, k_buf_sz> recv_buf{};
        for (std::span<uint8_t> view{recv_buf}; !view.empty();) {
            Status<std::span<uint8_t>, int> recv = socket.recv_some(view);
            AXLE_TEST_ASSERT_OK(recv);
            const std::span<uint8_t> recv_view = recv.ok();
            if (recv_view.empty()) {
                break;
            }

            view = view.subspan(recv_view.size());
        }
        ASSERT_EQ(payload, recv_buf);

        // Receive EOF
        const std::span<uint8_t> buf_view{recv_buf};
        Status<std::span<uint8_t>, int> recv = socket.recv_some(buf_view);
        AXLE_TEST_ASSERT_OK(recv);
        ASSERT_TRUE(recv.ok().empty());

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

TEST(AsyncClientSocketTest, ServerBlockingSendThenClose) {
    const int port = test_port();
    std::array<uint8_t, k_buf_sz> payload{};
    SendOnSignalServer server{port, payload};
    server.start();

    Scheduler::enter([port, &server, &payload] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        // Runs after the call to recv_some below blocks
        Scheduler::schedule([&server] { server.signal_send(); });

        // Receive payload
        std::array<uint8_t, k_buf_sz> recv_buf{};
        for (std::span<uint8_t> view{recv_buf}; !view.empty();) {
            Status<std::span<uint8_t>, int> recv = socket.recv_some(view);
            AXLE_TEST_ASSERT_OK(recv);
            const std::span<uint8_t> recv_view = recv.ok();
            if (recv_view.empty()) {
                break;
            }

            view = view.subspan(recv_view.size());
        }
        ASSERT_EQ(payload, recv_buf);

        // Runs after the call to recv_some below blocks
        Scheduler::schedule([&server] { server.signal_close(); });

        // Receive EOF
        const std::span<uint8_t> buf_view{recv_buf};
        Status<std::span<uint8_t>, int> recv = socket.recv_some(buf_view);
        AXLE_TEST_ASSERT_OK(recv);
        ASSERT_TRUE(recv.ok().empty());

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

// TODO (whalbawi): Flaky under ASan
TEST(AsyncClientSocketTest, DISABLED_ConnectSendClose) {
    const int port = test_port();
    ResetOnSignalServer server{port};

    server.start();
    Scheduler::enter([&server, port] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        Status<size_t, int> send_buf = socket::get_sndbuf_sz(socket.get_fd());
        AXLE_TEST_ASSERT_OK(send_buf);
        const size_t send_buf_sz = send_buf.ok();

        Status<size_t, int> recv_buf = socket::get_rcvbuf_sz(socket.get_fd());
        AXLE_TEST_ASSERT_OK(recv_buf);
        const size_t recv_buf_sz = recv_buf.ok();

        Scheduler::schedule([&server] { server.signal_close(); }, "close_server");
        std::vector<uint8_t> buf((send_buf_sz + recv_buf_sz) * 2, 0);
        rnd_buf(buf);
        Status<None, int> send = socket.send_all(buf);
        ASSERT_TRUE(send.is_err());
        const int err = send.err();
        // EPIPE if the server side recv buffer is empty. ECONNRESET, otherwise.
        // We cannot control which is which but the net result is the same.
        ASSERT_TRUE((err == ECONNRESET) || (err == EPIPE)) << err;

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

TEST(AsyncSocketTest, NoStaleReadRegistration) {
    const uint64_t timeout_ns = 2e8;
    const int port = test_port();
    std::array<uint8_t, k_buf_sz> payload{};

    SendOnSignalServer server{port, payload};
    server.start();

    Scheduler::enter([&] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        server.signal_send();

        // Receive payload
        std::array<uint8_t, k_buf_sz> recv_buf{};
        for (std::span<uint8_t> view{recv_buf}; !view.empty();) {
            Status<std::span<uint8_t>, int> recv = socket.recv_some(view);
            AXLE_TEST_ASSERT_OK(recv);
            const std::span<uint8_t> recv_view = recv.ok();
            if (recv_view.empty()) {
                break;
            }

            view = view.subspan(recv_view.size());
        }
        ASSERT_EQ(payload, recv_buf);

        bool timer_fired = false;
        auto ev = Scheduler::get_event_loop();
        const Gate gate{};
        Status timer = ev->register_timer(timeout_ns, false, [&timer_fired, gate](auto) {
            timer_fired = true;
            gate.post();
        });
        AXLE_TEST_ASSERT_OK(timer);

        server.signal_close();
        AXLE_TEST_ASSERT_OK(gate.wait());

        // In case of a stale registration, the next line will be executed before the timer fires.
        ASSERT_TRUE(timer_fired);

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

TEST(AsyncSocketTest, NoStaleWriteRegistration) {
    const uint64_t timeout_ns = 2e8;
    Status fd_status = socket::create_tcp();
    AXLE_TEST_ASSERT_OK(fd_status);

    const int fd = fd_status.ok();

    // Send a large buffer so that `send_all` blocks
    Status<size_t, int> sndbuf_sz_status = socket::get_sndbuf_sz(fd);
    AXLE_TEST_ASSERT_OK(sndbuf_sz_status);
    const size_t send_buf_sz = sndbuf_sz_status.ok();

    Status<size_t, int> rcvbuf_sz_status = socket::get_rcvbuf_sz(fd);
    AXLE_TEST_ASSERT_OK(rcvbuf_sz_status);
    const size_t recv_buf_sz = rcvbuf_sz_status.ok();

    const size_t buf_sz = 2 * (recv_buf_sz + send_buf_sz);

    const int port = test_port();
    std::vector<uint8_t> payload;
    payload.resize(buf_sz);

    RecvOnSignalServer server{port, payload};
    server.start();

    Scheduler::enter([&] {
        AsyncClientSocket socket{};
        AXLE_TEST_ASSERT_OK(socket.connect("127.0.0.1", port));

        Scheduler::schedule([&server] { server.signal_recv(); });

        std::vector<uint8_t> buf;
        buf.resize(buf_sz);
        Status send = socket.send_all(buf);
        AXLE_TEST_ASSERT_OK(send);
        ASSERT_EQ(payload, buf);

        bool timer_fired = false;
        auto ev = Scheduler::get_event_loop();
        Gate gate{};
        Status timer = ev->register_timer(timeout_ns, false, [&timer_fired, &gate](auto) {
            timer_fired = true;
            gate.post();
        });
        AXLE_TEST_ASSERT_OK(timer);

        server.signal_close();
        AXLE_TEST_ASSERT_OK(gate.wait());

        // In case of a stale registration, the next line will be executed before the timer fires.
        ASSERT_TRUE(timer_fired);

        AXLE_TEST_ASSERT_OK(socket.close());
        Scheduler::shutdown();
    });
    server.join();
}

} // namespace axle

// NOLINTEND(readability-function-cognitive-complexity)
