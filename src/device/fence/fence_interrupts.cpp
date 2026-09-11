#include "device/fence/fence_interrupts.hpp"

#include "device/host/host.hpp"
#include "device/threading/doorbell.hpp"

namespace av {

FenceInterrupts::FenceInterrupts(Host &host) : host_(host), lock_(nullptr), connected_(false) {}

FenceInterrupts::~FenceInterrupts()
{
    if (lock_ == nullptr) return;
    disconnect();
    host_.destroy_mutex(lock_);
}

Status FenceInterrupts::initialize()
{
    host_.require_main_loop("FenceInterrupts::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_);
}

Status FenceInterrupts::connect()
{
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
    host_.require_main_loop("FenceInterrupts::disconnect");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    if (connected_) host_.disconnect_fence_interrupts();
    connected_ = false;
    host_.unlock_mutex(lock_);
}

void FenceInterrupts::raise(uint32_t line)
{
    if (lock_ == nullptr || line >= Doorbell::pages) return;
    host_.lock_mutex(lock_);
    if (connected_) host_.raise_fence_interrupt(line);
    host_.unlock_mutex(lock_);
}

/* Read on the main loop, which alone changes it. */
bool FenceInterrupts::connected() const
{
    return connected_;
}

bool FenceInterrupts::invariants_hold() const
{
    return lock_ != nullptr || !connected_;
}

}  // namespace av
