#include "icd/opencl/cl_devices.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/avgpu_adapters.hpp"

namespace av::clicd {

ClDevices::ClDevices(BlockSource &memory, Kmt &kmt) : memory_(memory), kmt_(kmt), devices_(), count_(0u), left_out_(0u), learnt_(false)
{
    AV_SCOPE("ClDevices::ClDevices");
}

ClDevices::~ClDevices()
{
    AV_SCOPE("ClDevices::~ClDevices");
    for (uint32_t index = 0u; index < count_; ++index) devices_[index].destroy();
}

void ClDevices::learn(const AvgpuAdapters &adapters)
{
    AV_SCOPE("ClDevices::learn");
    if (learnt_) return;
    learnt_ = true;
    for (uint32_t index = 0u; index < adapters.count(); ++index) {
        const uint64_t luid = adapters.luid(index);
        /* A device is opened by its adapter's LUID; without one there is no
         * saying which adapter it would open. */
        if (luid == 0u || count_ == max_devices) {
            ++left_out_;
            continue;
        }
        /* Storage past count_ is empty, so the construct cannot be refused. */
        static_cast<void>(devices_[count_].construct(memory_, kmt_, luid));
        ++count_;
    }
}

bool ClDevices::learnt() const
{
    AV_SCOPE("ClDevices::learnt");
    return learnt_;
}

uint32_t ClDevices::count() const
{
    AV_SCOPE("ClDevices::count");
    return count_;
}

uint32_t ClDevices::left_out() const
{
    AV_SCOPE("ClDevices::left_out");
    return left_out_;
}

ClDevice &ClDevices::at(uint32_t index)
{
    AV_SCOPE("ClDevices::at");
    return devices_[index].object();
}

ClDevice *ClDevices::of(const ClObject &object)
{
    AV_SCOPE("ClDevices::of");
    for (uint32_t index = 0u; index < count_; ++index)
        if (devices_[index].object().is(object)) return &devices_[index].object();
    return nullptr;
}

ClDevice *ClDevices::running(const GuestClContext &context)
{
    AV_SCOPE("ClDevices::running");
    for (uint32_t index = 0u; index < count_; ++index)
        if (devices_[index].object().runs(context)) return &devices_[index].object();
    return nullptr;
}

ClDevice *ClDevices::on(uint64_t luid)
{
    AV_SCOPE("ClDevices::on");
    if (luid == 0u) return nullptr;
    for (uint32_t index = 0u; index < count_; ++index)
        if (devices_[index].object().luid() == luid) return &devices_[index].object();
    return nullptr;
}

void ClDevices::report(StateWriter &writer) const
{
    AV_SCOPE("ClDevices::report");
    writer.open("devices", 0u);
    writer.flag("learnt", learnt_);
    writer.number("left_out", left_out_);
    for (uint32_t index = 0u; index < count_; ++index) devices_[index].object().report(writer);
    writer.close();
}

/* The first count_ are constructed and the rest are not; no two devices are
 * on one adapter; nothing is made before learn. */
bool ClDevices::invariants_hold() const
{
    AV_SCOPE("ClDevices::invariants_hold");
    if (count_ > max_devices || (!learnt_ && (count_ != 0u || left_out_ != 0u))) return false;
    for (uint32_t index = 0u; index < max_devices; ++index) {
        if (devices_[index].constructed() != (index < count_)) return false;
        if (index >= count_) continue;
        if (!devices_[index].object().invariants_hold()) return false;
        for (uint32_t other = 0u; other < index; ++other)
            if (devices_[other].object().luid() == devices_[index].object().luid()) return false;
    }
    return true;
}

}  // namespace av::clicd
