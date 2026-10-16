#include "executor/host_device_renewal.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/renewable_device.hpp"

namespace av {

HostDeviceRenewal::HostDeviceRenewal(BlockSource &blocks, DriverLog &lines)
    : tenants_(blocks), lines_(lines), renewing_()
{
    AV_SCOPE("HostDeviceRenewal::HostDeviceRenewal");
}

bool HostDeviceRenewal::admit(WakeSignal &wake)
{
    AV_SCOPE("HostDeviceRenewal::admit");
    return tenants_.join(wake) == Status::ok;
}

void HostDeviceRenewal::dismiss(WakeSignal &wake)
{
    AV_SCOPE("HostDeviceRenewal::dismiss");
    tenants_.leave(wake);
}

bool HostDeviceRenewal::lost(RenewableDevice &device)
{
    AV_SCOPE("HostDeviceRenewal::lost");
    if (tenants_.lost()) return true;
    if (!device.removed()) return false;
    declare("its runtime reports it removed");
    return true;
}

void HostDeviceRenewal::declare(const char *why)
{
    AV_SCOPE("HostDeviceRenewal::declare");
    if (!tenants_.lose()) return;
    LogLine(lines_)
        .text("the device was lost (")
        .text(why)
        .text("): its ")
        .number(tenants_.count())
        .text(" sessions are faulted, and it is made again once they are gone")
        .write();
}

/* A device never made - open's, or a renewal's that failed - is made again
 * as a lost one is: the loss is latched first, so that once made, tenants
 * may join it. */
bool HostDeviceRenewal::renew(RenewableDevice &device, uint32_t wait_ms)
{
    AV_SCOPE("HostDeviceRenewal::renew");
    const ThreadLockHold one_at_a_time(renewing_);
    if (device.present() && !lost(device)) return true;
    if (!tenants_.vacate(wait_ms)) {
        LogLine(lines_).text("sessions are still on the lost device after ").number(wait_ms).text(" ms: not made again yet").write();
        return false;
    }
    static_cast<void>(tenants_.lose());
    const bool made = device.remake();
    if (made) static_cast<void>(tenants_.renew());
    lines_.write(made ? "the lost device was made again" : "the lost device could not be made again");
    return made;
}

bool HostDeviceRenewal::invariants_hold() const
{
    AV_SCOPE("HostDeviceRenewal::invariants_hold");
    return tenants_.invariants_hold();
}

}  // namespace av
