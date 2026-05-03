#pragma once

#include <liburing.h>

#include <cstdint>

#include <span>

namespace axle {

template <typename T, typename E>
class Status;

enum class None : uint8_t;

class Gate;

class IOUring {
  public:
    explicit IOUring(int queue_depth = k_default_queue_depth);
    ~IOUring();

    IOUring(const IOUring&) = delete;
    IOUring& operator=(const IOUring&) = delete;
    IOUring(IOUring&&) = delete;
    IOUring& operator=(IOUring&&) = delete;

    Status<None, int> submit_recv(int fd, std::span<uint8_t> buf_view, Gate& gate);

    void poll();

  private:
    static constexpr int k_default_queue_depth = 256;
    io_uring ring_;
};

} // namespace axle
