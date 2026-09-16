#include "core/memory/gpu_virtual_address_space.hpp"

#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

/* ------------------------------------------------------------ VirtualRange */

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
    /* Zero is how an unbound range is spelled, so it cannot name an object. */
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

/* -------------------------------------------------- GpuVirtualAddressSpace */

GpuVirtualAddressSpace::GpuVirtualAddressSpace(Platform &platform)
    : platform_(platform), ranges_(nullptr), count_(0u), capacity_(0u),
      ceiling_(maximum_range_count), bounds_(), page_(page_size)
      {
          AV_SCOPE("GpuVirtualAddressSpace::GpuVirtualAddressSpace");
      }

GpuVirtualAddressSpace::~GpuVirtualAddressSpace()
{
    AV_SCOPE("GpuVirtualAddressSpace::~GpuVirtualAddressSpace");
    release_table();
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
    return count_;
}

void GpuVirtualAddressSpace::set_range_ceiling(uint32_t ceiling)
{
    AV_SCOPE("GpuVirtualAddressSpace::set_range_ceiling");
    ceiling_ = ceiling;
}

uint32_t GpuVirtualAddressSpace::lower_bound(uint64_t base) const
{
    AV_SCOPE("GpuVirtualAddressSpace::lower_bound");
    uint32_t low = 0u, high = count_;
    while (low < high) {
        const uint32_t mid = low + (high - low) / 2u;
        if (ranges_[mid].span().base() < base) low = mid + 1u;
        else                                   high = mid;
    }
    return low;
}

VirtualRange *GpuVirtualAddressSpace::find_exact(uint64_t base)
{
    AV_SCOPE("GpuVirtualAddressSpace::find_exact");
    const uint32_t index = lower_bound(base);
    if (index < count_ && ranges_[index].span().base() == base)
        return &ranges_[index];
    return nullptr;
}

/* The table does not exist until the first grow, and the kernel bugchecks on
 * a null release. */
void GpuVirtualAddressSpace::release_table()
{
    AV_SCOPE("GpuVirtualAddressSpace::release_table");
    if (ranges_ != nullptr) platform_.release(ranges_);
}

Status GpuVirtualAddressSpace::grow()
{
    AV_SCOPE("GpuVirtualAddressSpace::grow");
    uint32_t wanted = (capacity_ == 0u) ? 16u : capacity_ * 2u;
    if (wanted > ceiling_) {
        if (capacity_ >= ceiling_) return Status::no_space;
        wanted = ceiling_;
    }
    const size_t bytes =
        static_cast<size_t>(wanted) * sizeof(VirtualRange);
    auto *fresh = static_cast<VirtualRange *>(
        platform_.allocate(bytes, Pool::non_paged));
    if (fresh == nullptr) return Status::no_memory;

    platform_.zero(fresh, bytes);
    if (count_ != 0u)
        platform_.copy(fresh, ranges_,
                       static_cast<size_t>(count_) * sizeof(VirtualRange));
    release_table();
    ranges_ = fresh;
    capacity_ = wanted;
    return Status::ok;
}

Status GpuVirtualAddressSpace::insert(uint32_t index,
                                      const VirtualRange &range)
{
    AV_SCOPE("GpuVirtualAddressSpace::insert");
    if (count_ == capacity_) {
        const Status status = grow();
        if (status != Status::ok) return status;
    }
    for (uint32_t i = count_; i > index; i--) ranges_[i] = ranges_[i - 1u];
    ranges_[index] = range;
    count_++;
    return Status::ok;
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
    if (!alignment.align_up(bounds_.base(), cursor)) return Status::no_space;

    for (uint32_t i = 0u; i <= count_; i++) {
        const uint64_t next =
            (i < count_) ? ranges_[i].span().base() : bounds_.end();
        if (cursor < next && next - cursor >= size) {
            const Status status = insert(i, VirtualRange(ByteRange(cursor, size)));
            if (status != Status::ok) return status;
            reserved_base = cursor;
            return Status::ok;
        }
        if (i < count_) {
            uint64_t after = 0u;
            if (!alignment.align_up(ranges_[i].span().end(), after))
                return Status::no_space;
            /* An aligned cursor past the end of the space cannot host a hole. */
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
    if (index >= count_ || ranges_[index].span().base() != base)
        return Status::not_found;
    if (ranges_[index].is_mapped()) return Status::invalid_state;

    for (uint32_t i = index; i + 1u < count_; i++) ranges_[i] = ranges_[i + 1u];
    count_--;
    ranges_[count_] = VirtualRange();
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
    /* lower_bound lands on the range starting at `address`, or the one after
     * it; in the latter case the only candidate is the range before. */
    if (index == count_ || ranges_[index].span().base() != address) {
        if (index == 0u) return Status::not_found;
        index--;
    }
    const VirtualRange &range = ranges_[index];
    if (!range.span().contains(address))              return Status::not_found;
    if (!range.translate(address, object, object_offset))
        return Status::invalid_state;
    return Status::ok;
}

bool GpuVirtualAddressSpace::invariants_hold() const
{
    AV_SCOPE("GpuVirtualAddressSpace::invariants_hold");
    if (count_ > capacity_)                     return false;
    if (capacity_ != 0u && ranges_ == nullptr)  return false;
    if (capacity_ > ceiling_)                   return false;
    if (ceiling_ == 0u || ceiling_ > maximum_range_count) return false;
    if (count_ != 0u && !bounds_.is_representable()) return false;

    uint64_t previous_end = bounds_.base();
    for (uint32_t i = 0u; i < count_; i++) {
        const VirtualRange &range = ranges_[i];
        if (!range.invariants_hold(bounds_, page_)) return false;
        if (range.span().base() < previous_end)     return false;
        previous_end = range.span().end();
    }
    return true;
}

}  // namespace av
