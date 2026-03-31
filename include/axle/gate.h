#pragma once
#include <cstddef>

#include <memory>

#include "axle/status.h"

namespace axle {

class Fiber;

class Gate {
  public:
    Gate();
    void post() const;

    Status<None, None> wait() const;

  private:
    struct State {
        std::shared_ptr<Fiber> waiting_fiber_;
        bool posted_{};
    };
    std::shared_ptr<State> state_;
};

} // namespace axle
