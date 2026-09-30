#include "device/fence/fence_interrupts.hpp"

#include "core/trace/function_scope.hpp"
#include "device/probe/interrupt_stress_probe.hpp"

#include "device/abi/rvk_device.h"
#include "device/host/host.hpp"

namespace av {

static_assert(FenceInterrupts::display_line == RVK_DEVICE_DISPLAY_LINE &&
                  FenceInterrupts::present_line == RVK_DEVICE_PRESENT_LINE &&
                  FenceInterrupts::lines == RVK_DEVICE_INTERRUPT_LINES,
              "the glue makes a virtqueue for every line the core raises");

FenceInterrupts::FenceInterrupts(Host &host) : host_(host), lock_(nullptr), connected_(false), stress_(nullptr)
{
    AV_SCOPE("FenceInterrupts::FenceInterrupts");
}

FenceInterrupts::~FenceInterrupts()
{
    AV_SCOPE("FenceInterrupts::~FenceInterrupts");
    if (lock_ == nullptr) return;
    disconnect();
    host_.destroy_mutex(lock_);
}

Status FenceInterrupts::initialize()
{
    AV_SCOPE("FenceInterrupts::initialize");
    host_.require_main_loop("FenceInterrupts::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_);
}

Status FenceInterrupts::connect()
{
    AV_SCOPE("FenceInterrupts::connect");
    host_.require_main_loop("FenceInterrupts::connect");
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    const Status status = connected_ ? Status::ok : host_.connect_fence_interrupts();
    connected_ = status == Status::ok;
    host_.unlock_mutex(lock_);
    return status;
}

void FenceInterrupts::disconnect()
{
    AV_SCOPE("FenceInterrupts::disconnect");
    host_.require_main_loop("FenceInterrupts::disconnect");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    if (connected_) host_.disconnect_fence_interrupts();
    connected_ = false;
    host_.unlock_mutex(lock_);
}

void FenceInterrupts::raise(uint32_t line)
{
    AV_SCOPE("FenceInterrupts::raise");
    if (lock_ == nullptr || line >= lines) return;
    host_.lock_mutex(lock_);
    const bool injected = connected_;
    if (injected) host_.raise_fence_interrupt(line);
    host_.unlock_mutex(lock_);
    /* Counted outside this lock: the stress probe holds its own, and nesting
     * them would put the interrupt path behind two. */
    if (injected && stress_ != nullptr) stress_->raised(line);
}

void FenceInterrupts::report_to(InterruptStressProbe &stress)
{
    AV_SCOPE("FenceInterrupts::report_to");
    host_.require_main_loop("FenceInterrupts::report_to");
    stress_ = &stress;
}

/* Read on the main loop, which alone changes it. */
bool FenceInterrupts::connected() const
{
    AV_SCOPE("FenceInterrupts::connected");
    return connected_;
}

bool FenceInterrupts::invariants_hold() const
{
    AV_SCOPE("FenceInterrupts::invariants_hold");
    return lock_ != nullptr || !connected_;
}

}  // namespace av
