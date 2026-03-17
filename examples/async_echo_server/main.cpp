#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>

#include <array>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <utility>

#include "axle/async_socket.h"
#include "axle/scheduler.h"
#include "axle/status.h"

namespace {

void connection_handler(const std::shared_ptr<axle::AsyncSocket>& socket) {
    constexpr size_t buf_sz = 16 * 1024UL;
    std::array<uint8_t, buf_sz> buf{};
    const std::span<uint8_t> buf_view{buf};

    for (;;) {
        axle::Status<std::span<uint8_t>, int> recv_status = socket->recv_some(buf_view);
        if (recv_status.is_err()) {
            std::cerr << "could not recv data from socket - error: " << recv_status.err() << "\n";
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

void server_loop(int port, int backlog) {
    const axle::AsyncServerSocket socket{};
    axle::Status<axle::None, int> listen_status = socket.listen(port, backlog);
    if (listen_status.is_err()) {
        std::cerr << "could not listen on socket - error: " << listen_status.err() << "\n";
        return;
    }

    std::cerr << "Echo server launched on 127.0.0.1:" << port << "\n";

    for (;;) {
        axle::Status<axle::AsyncSocket, int> accept_status = socket.accept();
        if (accept_status.is_err()) {
            std::cerr << "could not accept connection - error: " << accept_status.err() << "\n";
            return;
        }

        std::shared_ptr<axle::AsyncSocket> peer_socket =
            std::make_shared<axle::AsyncSocket>(accept_status.ok());

        axle::Scheduler::schedule([s = std::move(peer_socket)]() { connection_handler(s); },
                                  "connect");
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    try {
        constexpr int port = 8080;
        constexpr int backlog = 128;
        axle::Scheduler::enter([] { server_loop(port, backlog); });
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return -1;
    }
}
