#include <cstddef>
#include <cstdint>

#include <array>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <thread>
#include <utility>

#include "axle/async_socket.h"
#include "axle/scheduler.h"
#include "axle/socket.h"
#include "axle/status.h"

#include "benchmark/benchmark.h"

namespace axle {

template <size_t BufSz>
class EchoServer {
  public:
    explicit EchoServer(int port) : port_(port) {}

    void start() {
        const axle::AsyncServerSocket socket{};
        axle::Status<axle::None, int> listen_status = socket.listen(port_, k_listen_backlog);
        if (listen_status.is_err()) {
            std::cerr << "could not listen on socket - error: " << listen_status.err() << "\n";
            return;
        }

        running_ = true;
        while (running_) {
            axle::Status<axle::AsyncSocket, int> accept_status = socket.accept();
            if (accept_status.is_err()) {
                return;
            }

            std::shared_ptr<axle::AsyncSocket> peer_socket =
                std::make_shared<axle::AsyncSocket>(accept_status.ok());

            axle::Scheduler::schedule([s = std::move(peer_socket)]() { connection_handler(s); });
        }
    }

    void stop() {
        running_ = false;
    }

    bool running() {
        return running_;
    }

    static void connection_handler(const std::shared_ptr<axle::AsyncSocket>& socket) {
        std::array<uint8_t, BufSz> buf{};
        const std::span<uint8_t> buf_view{buf};

        for (;;) {
            axle::Status<std::span<uint8_t>, int> recv_status = socket->recv_some(buf_view);
            if (recv_status.is_err()) {
                (void)socket->close();
                return;
            }

            const std::span<uint8_t> recv_buf = recv_status.ok();
            if (recv_buf.empty()) {
                (void)socket->close();
                return;
            }

            const axle::Status<axle::None, int> send_status = socket->send_all(recv_buf);
            if (send_status.is_err()) {
                (void)socket->close();
                return;
            }
        }
    }

  private:
    static constexpr int k_listen_backlog = 128;

    std::shared_ptr<axle::Scheduler> scheduler_;
    int port_;
    std::atomic_bool running_{false};
};

} // namespace axle

template <size_t ServerBufSz, size_t ClientBufSz>
struct BenchEchoServer {
    void operator()(benchmark::State& state) {
        constexpr int port = 8080;
        axle::EchoServer<ServerBufSz> server{port};

        std::thread server_thread([&server] {
            axle::Scheduler::init();
            axle::Scheduler::schedule([&server] { server.start(); });
            axle::Scheduler::yield();
            axle::Scheduler::fini();
        });

        while (!server.running()) {
        };
        const axle::ClientSocket socket{};
        axle::Status<axle::None, int> connect_status = socket.connect("127.0.0.1", port);
        if (connect_status.is_err()) {
            std::cerr << "connect error: " << connect_status.err() << "\n";
            return;
        }

        std::array<uint8_t, ClientBufSz> send_buf{};
        std::array<uint8_t, ClientBufSz> recv_buf{};

        for (auto _ : state) {
            const axle::Status<axle::None, int> send_status =
                socket.send_all(std::span<uint8_t>(send_buf));
            if (send_status.is_err()) {
                return;
            }
            std::span<uint8_t> recv_view{recv_buf};
            while (!recv_view.empty()) {
                axle::Status<std::span<uint8_t>, int> recv_status = socket.recv_some(recv_view);
                if (recv_status.is_err()) {
                    return;
                }
                recv_view = recv_view.subspan(recv_status.ok().size());
            }
        }

        server.stop();
        axle::Scheduler::shutdown_all();
        server_thread.join();
    }
};

template <template <size_t, size_t> typename F>
class BenchmarkRegistrar {
  public:
    template <size_t... Is, size_t... Js>
    static void init(const std::string& suite_name,
                     std::index_sequence<Is...> /* unused */,
                     std::index_sequence<Js...> js) {
        (init_for<Is>(suite_name, js), ...);
    }

  private:
    template <std::size_t I>
    static void init_for(const std::string& /* unused */, std::index_sequence<> /* unused */) {}

    template <std::size_t I, std::size_t J, std::size_t... Js>
    static void init_for(const std::string& suite_name,
                         std::index_sequence<J, Js...> /* unused */) {
        init_impl<I, J>(suite_name);

        init_for<I>(suite_name, std::index_sequence<Js...>{});
    }

    template <std::size_t I, std::size_t J>
    static void init_impl(const std::string& suite_name) {
        const std::string test_name =
            (std::stringstream{} << suite_name << "/" << I << "/" << J).str();
        benchmark::RegisterBenchmark(test_name, F<I, J>{});
    }
};

constexpr size_t k_one_ki = 1024;

template <size_t... Is>
using ki_exp_seq = std::index_sequence<(k_one_ki * (1 << Is))...>;

template <typename Seq>
struct ki_exp_seq_helper;

template <size_t... Is>
struct ki_exp_seq_helper<std::index_sequence<Is...>> {
    using type = ki_exp_seq<Is...>;
};

template <size_t N>
using make_ki_exp_seq = ki_exp_seq_helper<std::make_index_sequence<N>>::type;

int main(int argc, char** argv) {
    try {
        BenchmarkRegistrar<BenchEchoServer>::init(
            "BM_EchoServer", make_ki_exp_seq<4>{}, make_ki_exp_seq<4>{});

        benchmark::Initialize(&argc, argv);
        (void)benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return -1;
    }
}
