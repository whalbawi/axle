#include "iouring.h"

#include <liburing.h>
#include <liburing/io_uring.h>

#include <system_error>

#include "axle/gate.h"
#include "axle/status.h"

namespace axle {

IOUring::IOUring(int queue_depth) : ring_{} {
    const int ret = io_uring_queue_init(queue_depth, &ring_, IORING_SETUP_SINGLE_ISSUER);
    if (ret < 0) {
        throw std::system_error{-ret, std::generic_category(), "io_uring init failed"};
    }
}

Status<None, int> IOUring::submit_recv(int fd, std::span<uint8_t> buf_view, Gate& gate) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (sqe == nullptr) {
        return Err(-1);
    }

    io_uring_prep_recv(sqe, fd, buf_view.data(), buf_view.size(), 0);
    io_uring_sqe_set_data(sqe, static_cast<void*>(&gate));

    return Ok();
}

void IOUring::poll() {
    uint32_t head = 0;
    const io_uring_cqe* cqe = nullptr;
    size_t count = 0;

    io_uring_for_each_cqe(&ring_, head, cqe) {
        Gate& gate = *static_cast<Gate*>(io_uring_cqe_get_data(cqe));
        gate.post();
        ++count;
    }
    io_uring_cq_advance(&ring_, count);

    const int ret = io_uring_submit(&ring_);
    if (ret < 0) {
        throw std::system_error{-ret, std::generic_category(), "io_uring submission failed"};
    }
}

} // namespace axle
