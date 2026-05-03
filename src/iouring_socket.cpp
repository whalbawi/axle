#include "iouring_socket.h"

#include <cerrno>

#include <span>

#include "debug.h"

namespace axle {

Status<None, int> CompletionAsyncSocket::send_all(std::span<const uint8_t> buf_view) const {
    while (!buf_view.empty()) {
        Status send_st = io_uring_->send(get_fd(), &buf_view, gate_);
        if (send_st.is_err()) {
            return Err(send_st.err());
        }
        AXLE_ASSERT(send_st.is_ok());

        Status wait_st = gate_.wait();
        if (wait_st.is_err()) {
            return Err(EINTR);
        }
    }

    return Ok();
}

template <typename G, typename F>
auto CompletionAsyncSocket::submit_and_wait(F&& f) {
    G gate;
    Status st = std::forward<F>(f)(gate);
    if (st.is_err()) {
        return Err(st.err());
    }

    AXLE_ASSERT(st.is_ok());

    Status wait_st = gate.wait();
    if (wait_st.is_err()) {
        return Err(EINTR);
    }

    AXLE_ASSERT(wait_st.is_ok());

    return Ok(st.ok());
}

Status<std::span<uint8_t>, int> CompletionAsyncSocket::recv_some(
    std::span<uint8_t> buf_view) const {
    auto submit = [this, &buf_view](Gate& gate) { io_uring_->recv(get_fd(), buf_view, gate); };

    return submit_and_wait<Gate>(submit);
}

} // namespace axle
