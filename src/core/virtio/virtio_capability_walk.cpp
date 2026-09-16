#include "core/virtio/virtio_capability_walk.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

enum : uint8_t {
    common_type        = 1u,
    notify_type        = 2u,
    isr_type           = 3u,
    device_type        = 4u,
    shared_memory_type = 8u,
};

constexpr uint8_t base_cap_len          = 16u;
constexpr uint8_t notify_cap_len        = 20u;
constexpr uint8_t shared_memory_cap_len = 24u;

}  // namespace

/* The structures found so far; the first instance of each wins (4.1.4.1). */
class VirtioCapabilityWalk::Collector {
public:
    Collector() : found_{}, multiplier_(0u), shared_{}, shared_ids_{}, shared_count_(0u)
    {
        AV_SCOPE("VirtioCapabilityWalk::Collector::Collector");
    }

    void keep(uint8_t type, uint8_t id, const VirtioRegion &region, uint32_t multiplier)
    {
        AV_SCOPE("VirtioCapabilityWalk::Collector::keep");
        if (type >= common_type && type <= device_type && !found_[type].present()) {
            found_[type] = region;
            if (type == notify_type) multiplier_ = multiplier;
        }
        if (type == shared_memory_type && shared_count_ < VirtioPciLayout::max_shared_memory_regions &&
            !has_shared(id)) {
            shared_[shared_count_] = region;
            shared_ids_[shared_count_++] = id;
        }
    }

    Status build(VirtioPciLayout &layout) const
    {
        AV_SCOPE("VirtioCapabilityWalk::Collector::build");
        for (uint8_t type = common_type; type <= device_type; type++)
            if (!found_[type].present()) return Status::not_supported;
        /* 4.1.4.4: 0 or an even power of two. */
        const bool multiplier_ok =
            multiplier_ == 0u || (multiplier_ >= 2u && (multiplier_ & (multiplier_ - 1u)) == 0u);
        if (!multiplier_ok || found_[common_type].length() < common_bytes ||
            found_[notify_type].length() < notify_bytes || found_[isr_type].length() < isr_bytes)
            return Status::malformed;
        layout = VirtioPciLayout(found_[common_type], found_[notify_type], found_[isr_type],
                                 found_[device_type], multiplier_, shared_, shared_ids_, shared_count_);
        return Status::ok;
    }

private:
    bool has_shared(uint8_t id) const
    {
        AV_SCOPE("VirtioCapabilityWalk::Collector::has_shared");
        for (uint8_t i = 0u; i < shared_count_; i++)
            if (shared_ids_[i] == id) return true;
        return false;
    }

    VirtioRegion found_[device_type + 1u];
    uint32_t     multiplier_;
    VirtioRegion shared_[VirtioPciLayout::max_shared_memory_regions];
    uint8_t      shared_ids_[VirtioPciLayout::max_shared_memory_regions];
    uint8_t      shared_count_;
};

VirtioCapabilityWalk::VirtioCapabilityWalk(const uint8_t *config_space, size_t length)
    : bytes_{}, complete_(config_space != nullptr && length >= config_space_bytes)
{
    AV_SCOPE("VirtioCapabilityWalk::VirtioCapabilityWalk");
    for (size_t i = 0u; complete_ && i < config_space_bytes; i++) bytes_[i] = config_space[i];
}

uint32_t VirtioCapabilityWalk::read_u32(size_t offset) const
{
    AV_SCOPE("VirtioCapabilityWalk::read_u32");
    return static_cast<uint32_t>(bytes_[offset]) | static_cast<uint32_t>(bytes_[offset + 1u]) << 8u |
           static_cast<uint32_t>(bytes_[offset + 2u]) << 16u |
           static_cast<uint32_t>(bytes_[offset + 3u]) << 24u;
}

Status VirtioCapabilityWalk::visit(size_t at, Collector &collector) const
{
    AV_SCOPE("VirtioCapabilityWalk::visit");
    if (bytes_[at] != vendor_capability) return Status::ok;
    const uint8_t cap_len = bytes_[at + 2u];
    const uint8_t type = bytes_[at + 3u];
    const bool too_short = cap_len < base_cap_len ||
                           (type == notify_type && cap_len < notify_cap_len) ||
                           (type == shared_memory_type && cap_len < shared_memory_cap_len);
    if (too_short || at + cap_len > config_space_bytes) return Status::malformed;
    const uint8_t bar = bytes_[at + 4u];
    if (bar > highest_bar) return Status::ok;   /* reserved BAR: ignore (4.1.4.1) */

    uint64_t offset = read_u32(at + 8u);
    uint64_t length = read_u32(at + 12u);
    if (type == shared_memory_type) {
        offset |= uint64_t{read_u32(at + 16u)} << 32u;
        length |= uint64_t{read_u32(at + 20u)} << 32u;
    }
    if (offset + length < offset) return Status::malformed;
    const uint32_t multiplier = (type == notify_type) ? read_u32(at + 16u) : 0u;
    collector.keep(type, bytes_[at + 5u], VirtioRegion(bar, offset, length), multiplier);
    return Status::ok;
}

Status VirtioCapabilityWalk::walk(VirtioPciLayout &layout) const
{
    AV_SCOPE("VirtioCapabilityWalk::walk");
    if (!complete_) return Status::malformed;
    if ((bytes_[status_register] & has_capabilities) == 0u) return Status::not_supported;
    Collector collector;
    bool visited[config_space_bytes] = {};
    for (size_t at = bytes_[capability_pointer] & 0xFCu; at != 0u; at = bytes_[at + 1u] & 0xFCu) {
        if (at < first_capability || visited[at]) return Status::malformed;
        visited[at] = true;
        const Status status = visit(at, collector);
        if (status != Status::ok) return status;
    }
    return collector.build(layout);
}

}  // namespace av
