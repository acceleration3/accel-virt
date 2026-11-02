#include "device/feasibility/interrupt_stress_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

InterruptStressProbe::InterruptStressProbe(Host &host)
    : host_(host), lock_(nullptr), raised_(0u), seen_(0u), worst_(0u), injected_while_disabled_(0u),
      listening_(true), result_()
      {
          AV_SCOPE("InterruptStressProbe::InterruptStressProbe");
      }

InterruptStressProbe::~InterruptStressProbe()
{
    AV_SCOPE("InterruptStressProbe::~InterruptStressProbe");
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status InterruptStressProbe::initialize()
{
    AV_SCOPE("InterruptStressProbe::initialize");
    host_.require_main_loop("InterruptStressProbe::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_);
}

void InterruptStressProbe::raised(uint32_t)
{
    AV_SCOPE("InterruptStressProbe::raised");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    raised_++;
    if (!listening_) injected_while_disabled_++;
    host_.unlock_mutex(lock_);
}

void InterruptStressProbe::disabled()
{
    AV_SCOPE("InterruptStressProbe::disabled");
    host_.require_main_loop("InterruptStressProbe::disabled");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    listening_ = false;
    host_.unlock_mutex(lock_);
}

void InterruptStressProbe::enabled()
{
    AV_SCOPE("InterruptStressProbe::enabled");
    host_.require_main_loop("InterruptStressProbe::enabled");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    listening_ = true;
    host_.unlock_mutex(lock_);
}

void InterruptStressProbe::acknowledged(uint64_t seen, uint64_t nanoseconds)
{
    AV_SCOPE("InterruptStressProbe::acknowledged");
    host_.require_main_loop("InterruptStressProbe::acknowledged");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    seen_ = seen;
    if (nanoseconds > worst_) worst_ = nanoseconds;
    host_.unlock_mutex(lock_);
}

void InterruptStressProbe::finish()
{
    AV_SCOPE("InterruptStressProbe::finish");
    host_.require_main_loop("InterruptStressProbe::finish");
    if (lock_ == nullptr || result_.ran()) return;
    host_.lock_mutex(lock_);
    const uint64_t raised = raised_;
    const uint64_t seen = seen_;
    const uint64_t injected = injected_while_disabled_;
    const uint64_t worst = worst_;
    host_.unlock_mutex(lock_);
    if (injected != 0u) {
        static_cast<void>(result_.record_failure(Status::invalid_state, static_cast<int32_t>(injected)));
        return;
    }
    if (seen < raised) {
        static_cast<void>(result_.record_failure(Status::not_found, static_cast<int32_t>(raised - seen)));
        return;
    }
    if (seen > raised) {
        static_cast<void>(result_.record_failure(Status::malformed, static_cast<int32_t>(seen - raised)));
        return;
    }
    static_cast<void>(result_.record_pass(worst));
}

const ProbeResult &InterruptStressProbe::verdict() const
{
    AV_SCOPE("InterruptStressProbe::verdict");
    return result_;
}

}
