#include "miniport/memory/gpu_virtual_address_space.hpp"
#include "core/support/sorted_search.hpp"

#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

VirtualRange::VirtualRange()
    : span_(), object_(0u), object_offset_(0u), mapped_(false)
    {
        AV_SCOPE("VirtualRange::VirtualRange");
    }

VirtualRange::VirtualRange(const ByteRange &span)
    : span_(span), object_(0u), object_offset_(0u), mapped_(false)
    {
        AV_SCOPE("VirtualRange::VirtualRange");
    }

const ByteRange &VirtualRange::span() const
{
    AV_SCOPE("VirtualRange::span");
    return span_;
}
bool             VirtualRange::is_mapped() const
{
    AV_SCOPE("VirtualRange::is_mapped");
    return mapped_;
}
uint32_t    VirtualRange::object() const
{
    AV_SCOPE("VirtualRange::object");
    return object_;
}
uint64_t    VirtualRange::object_offset() const
{
    AV_SCOPE("VirtualRange::object_offset");
    return object_offset_;
}

Status VirtualRange::bind(uint32_t object, uint64_t object_offset)
{
    AV_SCOPE("VirtualRange::bind");
    if (object == 0u) return Status::invalid_parameter;
    if (!ByteRange(object_offset, span_.size()).is_representable())
        return Status::invalid_parameter;
    if (mapped_) return Status::conflict;

    object_ = object;
    object_offset_ = object_offset;
    mapped_ = true;
    return Status::ok;
}

Status VirtualRange::unbind()
{
    AV_SCOPE("VirtualRange::unbind");
    if (!mapped_) return Status::invalid_state;
    object_ = 0u;
    object_offset_ = 0u;
    mapped_ = false;
    return Status::ok;
}

bool VirtualRange::translate(uint64_t address, uint32_t &object,
                             uint64_t &object_offset) const
{
    AV_SCOPE("VirtualRange::translate");
    if (!mapped_ || !span_.contains(address)) return false;
    object = object_;
    object_offset = object_offset_ + (address - span_.base());
    return true;
}

bool VirtualRange::invariants_hold(const ByteRange &bounds,
                                   const Alignment &page) const
{
    AV_SCOPE("VirtualRange::invariants_hold");
    if (!span_.is_representable())             return false;
    if (!page.divides(span_.base()))           return false;
    if (!page.divides(span_.size()))           return false;
    if (!bounds.encloses(span_))               return false;
    if (mapped_ != (object_ != 0u))            return false;
    if (!mapped_ && object_offset_ != 0u)      return false;
    if (!ByteRange(object_offset_, span_.size()).is_representable())
        return false;
    return true;
}

GpuVirtualAddressSpace::GpuVirtualAddressSpace(Platform &platform)
    : platform_(platform), ranges_(platform, Pool::non_paged), packed_(0u), bounds_(), page_(page_size)
{
    AV_SCOPE("GpuVirtualAddressSpace::GpuVirtualAddressSpace");
}

GpuVirtualAddressSpace::~GpuVirtualAddressSpace()
{
    AV_SCOPE("GpuVirtualAddressSpace::~GpuVirtualAddressSpace");
}

Status GpuVirtualAddressSpace::initialize(const ByteRange &bounds)
{
    AV_SCOPE("GpuVirtualAddressSpace::initialize");
    platform_.require_irql_at_most(Irql::passive, "GpuVirtualAddressSpace::initialize");
    if (!bounds.is_representable())       return Status::invalid_parameter;
    if (!page_.divides(bounds.base()))    return Status::invalid_parameter;
    if (!page_.divides(bounds.size()))    return Status::invalid_parameter;

    bounds_ = bounds;
    return Status::ok;
}

uint32_t GpuVirtualAddressSpace::range_count() const
{
    AV_SCOPE("GpuVirtualAddressSpace::range_count");
    return ranges_.size();
}

uint32_t GpuVirtualAddressSpace::lower_bound(uint64_t base) const
{
    AV_SCOPE("GpuVirtualAddressSpace::lower_bound");
    return SortedSearch<VirtualRange, [](const VirtualRange &range) { return range.span().base(); }>(ranges_)
        .at_or_after(base);
}

VirtualRange *GpuVirtualAddressSpace::find_exact(uint64_t base)
{
    AV_SCOPE("GpuVirtualAddressSpace::find_exact");
    const uint32_t index = lower_bound(base);
    if (index < ranges_.size() && ranges_.at(index).span().base() == base)
        return &ranges_.at(index);
    return nullptr;
}

uint64_t GpuVirtualAddressSpace::packed_end() const
{
    AV_SCOPE("GpuVirtualAddressSpace::packed_end");
    return packed_ == 0u ? bounds_.base() : ranges_.at(packed_ - 1u).span().end();
}

void GpuVirtualAddressSpace::extend_packed()
{
    AV_SCOPE("GpuVirtualAddressSpace::extend_packed");
    while (packed_ < ranges_.size() && ranges_.at(packed_).span().base() == packed_end()) packed_++;
}

Status GpuVirtualAddressSpace::reserve(uint64_t size,
                                       const Alignment &alignment,
                                       uint64_t &reserved_base)
{
    AV_SCOPE("GpuVirtualAddressSpace::reserve");
    platform_.require_irql_at_most(Irql::passive, "GpuVirtualAddressSpace::reserve");
    InvariantScope<GpuVirtualAddressSpace> guard(platform_, *this, "reserve");

    if (!alignment.is_valid())            return Status::invalid_parameter;
    if (alignment.value() < page_size)    return Status::invalid_parameter;
    if (size == 0u || !page_.divides(size)) return Status::invalid_parameter;
    if (size > bounds_.size())            return Status::too_large;

    uint64_t cursor = 0u;
    if (!alignment.align_up(packed_end(), cursor)) return Status::no_space;

    const uint32_t count = ranges_.size();
    for (uint32_t i = packed_; i <= count; i++) {
        const uint64_t next =
            (i < count) ? ranges_.at(i).span().base() : bounds_.end();
        if (cursor < next && next - cursor >= size) {
            const Status status = ranges_.insert_at(i, VirtualRange(ByteRange(cursor, size)));
            if (status != Status::ok) return status;
            extend_packed();
            reserved_base = cursor;
            return Status::ok;
        }
        if (i < count) {
            uint64_t after = 0u;
            if (!alignment.align_up(ranges_.at(i).span().end(), after))
                return Status::no_space;
            if (after >= bounds_.end()) return Status::no_space;
            if (after > cursor) cursor = after;
        }
    }
    return Status::no_space;
}

Status GpuVirtualAddressSpace::map(uint64_t base, uint64_t size,
                                   uint32_t object,
                                   uint64_t object_offset)
{
    AV_SCOPE("GpuVirtualAddressSpace::map");
    platform_.require_irql_at_most(Irql::passive, "GpuVirtualAddressSpace::map");
    InvariantScope<GpuVirtualAddressSpace> guard(platform_, *this, "map");

    VirtualRange *range = find_exact(base);
    if (range == nullptr)              return Status::not_found;
    if (range->span().size() != size)  return Status::invalid_parameter;
    return range->bind(object, object_offset);
}

Status GpuVirtualAddressSpace::unmap(uint64_t base)
{
    AV_SCOPE("GpuVirtualAddressSpace::unmap");
    platform_.require_irql_at_most(Irql::passive, "GpuVirtualAddressSpace::unmap");
    InvariantScope<GpuVirtualAddressSpace> guard(platform_, *this, "unmap");

    VirtualRange *range = find_exact(base);
    if (range == nullptr) return Status::not_found;
    return range->unbind();
}

Status GpuVirtualAddressSpace::release(uint64_t base)
{
    AV_SCOPE("GpuVirtualAddressSpace::release");
    platform_.require_irql_at_most(Irql::passive, "GpuVirtualAddressSpace::release");
    InvariantScope<GpuVirtualAddressSpace> guard(platform_, *this, "release");

    const uint32_t index = lower_bound(base);
    if (index >= ranges_.size() || ranges_.at(index).span().base() != base)
        return Status::not_found;
    if (ranges_.at(index).is_mapped()) return Status::invalid_state;

    ranges_.remove_at(index);
    if (index < packed_) packed_ = index;
    return Status::ok;
}

Status GpuVirtualAddressSpace::translate(uint64_t address,
                                         uint32_t &object,
                                         uint64_t &object_offset) const
{
    AV_SCOPE("GpuVirtualAddressSpace::translate");
    platform_.require_irql_at_most(Irql::dispatch,
                                   "GpuVirtualAddressSpace::translate");

    uint32_t index = lower_bound(address);
    if (index == ranges_.size() || ranges_.at(index).span().base() != address) {
        if (index == 0u) return Status::not_found;
        index--;
    }
    const VirtualRange &range = ranges_.at(index);
    if (!range.span().contains(address))              return Status::not_found;
    if (!range.translate(address, object, object_offset))
        return Status::invalid_state;
    return Status::ok;
}

bool GpuVirtualAddressSpace::invariants_hold() const
{
    AV_SCOPE("GpuVirtualAddressSpace::invariants_hold");
    if (!ranges_.invariants_hold() || packed_ > ranges_.size()) return false;
    if (ranges_.size() != 0u && !bounds_.is_representable()) return false;

    uint64_t previous_end = bounds_.base();
    for (uint32_t i = 0u; i < ranges_.size(); i++) {
        const VirtualRange &range = ranges_.at(i);
        if (i < packed_ && range.span().base() != previous_end) return false;
        if (!range.invariants_hold(bounds_, page_)) return false;
        if (range.span().base() < previous_end)     return false;
        previous_end = range.span().end();
    }
    return true;
}

}
