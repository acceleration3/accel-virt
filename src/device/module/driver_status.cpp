#include "device/module/driver_status.hpp"

#include "core/support/status.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/host/host.hpp"

namespace av {

DriverStatus::DriverStatus(Host &host, FenceInterrupts &interrupts)
    : host_(host), interrupts_(interrupts), value_(0u)
    {
        AV_SCOPE("DriverStatus::DriverStatus");
    }

void DriverStatus::set(uint8_t status)
{
    AV_SCOPE("DriverStatus::set");
    host_.require_main_loop("DriverStatus::set");
    value_ = status;
    if ((status & driver_ok) == 0u) {
        interrupts_.disconnect();
        return;
    }
    if (interrupts_.connect() == Status::ok) return;
    char       bytes[112];
    TextBuffer message(bytes, sizeof bytes);
    message.append("virtio-gpu-av: the fence interrupts could not be connected for status ");
    message.append_decimal(status);
    host_.log(message.text());
}

void DriverStatus::reset()
{
    AV_SCOPE("DriverStatus::reset");
    host_.require_main_loop("DriverStatus::reset");
    value_ = 0u;
    interrupts_.disconnect();
}

uint8_t DriverStatus::value() const
{
    AV_SCOPE("DriverStatus::value");
    return value_;
}

/* Interrupts connected while DRIVER_OK is not held would outlive the driver
 * that asked for them, which is the abort 035's live run hit. */
bool DriverStatus::invariants_hold() const
{
    AV_SCOPE("DriverStatus::invariants_hold");
    return !interrupts_.connected() || (value_ & driver_ok) != 0u;
}

}  // namespace av
