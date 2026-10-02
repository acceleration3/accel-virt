#include "icd/d3d/adapter_identity.hpp"

#include "core/log/driver_log.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "core/log/log_sink.hpp"

namespace av::d3d {
namespace {

constexpr char     adapter_name[] = "avgpu paravirtual GPU";

}  // namespace

AdapterIdentity::AdapterIdentity()
    : luid_(0ull), vendor_id_(0u), device_id_(0u), dedicated_(0ull), shared_(0ull), description_{}
{
    AV_SCOPE("AdapterIdentity::AdapterIdentity");
}

Status AdapterIdentity::learn(Kmt &kmt, uint32_t adapter, LogSink &log)
{
    AV_SCOPE("AdapterIdentity::learn");
    uint64_t     luid = 0ull;
    const Status had_luid = kmt.adapter_luid(adapter, luid);
    if (had_luid != Status::ok) {
        DriverLog(log, "avumd: ").note("adapter luid refused, status", static_cast<uint64_t>(had_luid));
        return Status::not_supported;
    }
    PciIdentity  identity;
    const Status had_identity = kmt.query_identity(adapter, identity);
    if (had_identity != Status::ok) {
        DriverLog(log, "avumd: ").note("adapter identity refused, status", static_cast<uint64_t>(had_identity));
        return Status::not_supported;
    }
    luid_ = luid;
    vendor_id_ = identity.vendor();
    device_id_ = identity.device();
    dedicated_ = budget(kmt, adapter, log, static_cast<uint32_t>(MemorySegmentGroup::local));
    shared_ = budget(kmt, adapter, log, static_cast<uint32_t>(MemorySegmentGroup::non_local));
    TextBuffer text(description_, sizeof description_);
    text.append(adapter_name);
    return Status::ok;
}

uint64_t AdapterIdentity::budget(Kmt &kmt, uint32_t adapter, LogSink &log, uint32_t group)
{
    AV_SCOPE("AdapterIdentity::budget");
    uint64_t     bytes = 0ull;
    uint64_t     used = 0ull;
    const Status asked = kmt.query_video_memory(adapter, static_cast<MemorySegmentGroup>(group), bytes, used);
    if (asked == Status::ok) return bytes;
    DriverLog(log, "avumd: ").note("video memory refused for group", group);
    return 0ull;
}

uint64_t AdapterIdentity::luid() const
{
    AV_SCOPE("AdapterIdentity::luid");
    return luid_;
}

uint32_t AdapterIdentity::vendor_id() const
{
    AV_SCOPE("AdapterIdentity::vendor_id");
    return vendor_id_;
}

uint32_t AdapterIdentity::device_id() const
{
    AV_SCOPE("AdapterIdentity::device_id");
    return device_id_;
}

uint64_t AdapterIdentity::dedicated_video_memory() const
{
    AV_SCOPE("AdapterIdentity::dedicated_video_memory");
    return dedicated_;
}

uint64_t AdapterIdentity::shared_system_memory() const
{
    AV_SCOPE("AdapterIdentity::shared_system_memory");
    return shared_;
}

const char *AdapterIdentity::description() const
{
    AV_SCOPE("AdapterIdentity::description");
    return description_;
}

void AdapterIdentity::report(StateWriter &writer) const
{
    AV_SCOPE("AdapterIdentity::report");
    writer.hex("luid", luid_);
    writer.hex("vendor", vendor_id_);
    writer.hex("device", device_id_);
    writer.number("dedicated", dedicated_);
    writer.number("shared", shared_);
}

bool AdapterIdentity::invariants_hold() const
{
    AV_SCOPE("AdapterIdentity::invariants_hold");
    /* Nothing is learned until the LUID is, so ids and a description without
     * one would mean a half-written identity a caller could report from. */
    if (luid_ == 0ull && (vendor_id_ != 0u || device_id_ != 0u || description_[0] != '\0')) return false;
    if (luid_ != 0ull && description_[0] == '\0') return false;
    for (uint32_t index = 0u; index < description_bytes; index++)
        if (description_[index] == '\0') return true;
    return false;
}

}  // namespace av::d3d
