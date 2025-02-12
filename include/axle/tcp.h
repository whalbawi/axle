#pragma once

#include <cerrno>
#include <cstdint>

#include <memory>
#include <span>
#include <utility>

#include "axle/event.h"
#include "axle/socket.h"
#include "axle/status.h"

namespace axle {

template <typename SessionT>
class TcpServer {
  public:
    TcpServer() = delete;
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    explicit TcpServer(std::shared_ptr<axle::EventLoop> event_loop, int port)
        : port_(port),
          event_loop_{std::move(event_loop)},
          socket_{axle::ServerSocket()},
          running_{false} {}

    virtual ~TcpServer() {
        (void)socket_.close();
    }

    virtual std::shared_ptr<SessionT> handle_connection() = 0;

    void start() {
        if (socket_.listen(port_, k_listen_backlog).is_err()) {
            return;
        }

        if (socket_.set_non_blocking().is_err()) {
            return;
        }

        (void)event_loop_->register_fd_read(
            socket_.get_fd(), [this](axle::Status<int64_t, uint32_t> status) {
                if (status.is_err()) {
                    return;
                }
                for (int64_t i = 0; i < status.ok(); ++i) {
                    axle::Status<axle::Socket, int> accept_status = socket_.accept();
                    if (accept_status.is_err()) {
                        continue;
                    }

                    setup_handlers(accept_status.ok());
                }
            });

        running_.store(true);
    }

    void setup_handlers(axle::Socket&& peer_socket) {
        const std::shared_ptr<axle::Socket> socket =
            std::make_shared<axle::Socket>(std::move(peer_socket));
        const std::shared_ptr<SessionT> session{handle_connection()};

        (void)event_loop_->register_fd_read(
            socket->get_fd(), [socket, session](axle::Status<int64_t, uint32_t> status) {
                if (status.is_err()) {
                    return;
                }

                const std::span<uint8_t> buf = session->recv_buf(status.ok());
                axle::Status<std::span<uint8_t>, int> res = socket->recv_some(buf);
                if (res.is_err()) {
                    return;
                }
                session->post_recv(res.ok());
            });

        (void)event_loop_->register_fd_write(
            socket->get_fd(), [socket, session](axle::Status<int64_t, uint32_t> status) {
                if (status.is_err()) {
                    return;
                }

                const std::span<const uint8_t> buf = session->send_buf(status.ok());
                const axle::Status<axle::None, int> res = socket->send_all(buf);
                if (res.is_err()) {
                    return;
                }
                session->post_send(buf.size());
            });

        (void)event_loop_->register_fd_eof(socket->get_fd(), [session]() { session->end(); });
    }

    bool running() {
        return running_.load();
    }

    void stop() {
        running_.store(false);
    }

  private:
    static constexpr int k_listen_backlog = 128;

    int port_;
    std::shared_ptr<axle::EventLoop> event_loop_;
    axle::ServerSocket socket_;
    std::atomic_bool running_;
};

} // namespace axle
