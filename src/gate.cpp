#include "axle/gate.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "debug.h"
#include "axle/scheduler.h"
#include "axle/status.h"

namespace axle {

Gate::Gate() : state_(std::make_shared<State>()) {}

void Gate::post() const {
    if (state_->posted_) {
        return;
    }

    state_->posted_ = true;
    if (state_->waiting_fiber_) {
        Scheduler::resume(std::exchange(state_->waiting_fiber_, nullptr));
    }
}

Status<None, None> Gate::wait() const {
    if (state_->waiting_fiber_) {
        throw std::logic_error{"Gate already waited on"};
    }

    if (state_->posted_) {
        state_->posted_ = false;
        return Ok();
    }

    state_->waiting_fiber_ = Scheduler::current_fiber();
    Scheduler::yield();
    if (Scheduler::current_fiber()->interrupted()) {
        return Err();
    }
    AXLE_ASSERT(state_->waiting_fiber_ == nullptr);

    state_->posted_ = false;
    return Ok();
}

} // namespace axle
