#include "icd/opencl/cl_device.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/opencl/guest_cl_context.hpp"

namespace av::clicd {

ClDevice::ClDevice(BlockSource &memory, Kmt &kmt, uint64_t luid)
    : luid_(luid), connection_(memory, kmt, luid), report_(luid), extensions_(reported_extensions, reported_extension_count),
      object_(nullptr)
{
    AV_SCOPE("ClDevice::ClDevice");
}

uint64_t ClDevice::luid() const
{
    AV_SCOPE("ClDevice::luid");
    return luid_;
}

icd::IcdConnection &ClDevice::connection()
{
    AV_SCOPE("ClDevice::connection");
    return connection_;
}

ClDeviceReport &ClDevice::decided()
{
    AV_SCOPE("ClDevice::decided");
    return report_;
}

ClExtensionPolicy &ClDevice::extensions()
{
    AV_SCOPE("ClDevice::extensions");
    return extensions_;
}

ClObject *ClDevice::object() const
{
    AV_SCOPE("ClDevice::object");
    return object_;
}

void ClDevice::opened(ClObject &object)
{
    AV_SCOPE("ClDevice::opened");
    object_ = &object;
}

bool ClDevice::is(const ClObject &object) const
{
    AV_SCOPE("ClDevice::is");
    return object_ == &object;
}

bool ClDevice::runs(const GuestClContext &context) const
{
    AV_SCOPE("ClDevice::runs");
    return context.on(connection_);
}

void ClDevice::report(StateWriter &writer) const
{
    AV_SCOPE("ClDevice::report");
    writer.open("device", 0u);
    writer.number("luid", luid_);
    writer.flag("open", object_ != nullptr);
    connection_.report(writer);
    writer.close();
}

/* A device is shown only once it is open, which takes its adapter: an open
 * device's connection holds a link. Its report names its own adapter. */
bool ClDevice::invariants_hold() const
{
    AV_SCOPE("ClDevice::invariants_hold");
    if (luid_ == 0u) return false;
    if (!connection_.invariants_hold() || !report_.invariants_hold() || !extensions_.invariants_hold()) return false;
    if (object_ != nullptr && connection_.users() == 0u) return false;
    return report_.names_adapter(luid_);
}

}  // namespace av::clicd
