#pragma once

namespace axle {

enum class PollState : uint8_t {
    OK,
    ERR,
};

enum class PollEvent : uint8_t {
    USER = 0,
    FD_READ,
    FD_WRITE,
    FD_EOF,
    TIMER,
    SIGNAL,
};

struct PollOutcome {
    int id;
    PollEvent event;
    PollState state;
};

} // namespace axle
