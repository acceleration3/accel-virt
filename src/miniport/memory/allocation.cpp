#include "miniport/memory/allocation.hpp"

#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

Allocation::Allocation(Platform &platform, const MemorySegments &segments,
                       const AllocationDescriptor &descriptor, uint64_t size)
    : platform_(platform), segments_(segments), descriptor_(descriptor), size_(size), opens_(0u), shared_id_(0u), resource_(nullptr), owner_(nullptr), process_(nullptr),
      aperture_(platform, static_cast<uint32_t>(size / page_bytes)), writes_(platform)
      {
          AV_SCOPE("Allocation::Allocation");
      }

const AllocationDescriptor &Allocation::descriptor() const
{
    AV_SCOPE("Allocation::descriptor");
    return descriptor_;
}
uint64_t                    Allocation::size() const
{
    AV_SCOPE("Allocation::size");
    return size_;
}
uint64_t                    Allocation::shared_id() const
{
    AV_SCOPE("Allocation::shared_id");
    return shared_id_;
}
Resource *Allocation::resource() const
{
    AV_SCOPE("Allocation::resource");
    return resource_;
}

void Allocation::belongs_to(Resource *resource)
{
    AV_SCOPE("Allocation::belongs_to");
    resource_ = resource;
}

PresentWrites &Allocation::writes()
{
    AV_SCOPE("Allocation::writes");
    return writes_;
}

void                        Allocation::shared_as(uint64_t id)
{
    AV_SCOPE("Allocation::shared_as");
    shared_id_ = id;
}

uint64_t                    Allocation::host_key() const
{
    AV_SCOPE("Allocation::host_key");
    return shared_id_ != 0u ? shared_id_ : descriptor_.host_key();
}
uint32_t                    Allocation::opens() const
{
    AV_SCOPE("Allocation::opens");
    return opens_;
}

void Allocation::report_to(DxgkAllocationInfoRecord &info)
{
    AV_SCOPE("Allocation::report_to");
    platform_.require_irql_at_most(Irql::apc, "Allocation::report_to");
    info.Size = size_;
    info.Alignment = 0u;
    info.PitchAlignedSize = 0u;
    info.HintedBank = 0u;
    const bool scanned_out = descriptor_.kind() == AllocationKind::shared_primary;
    const AllocationKind kind = descriptor_.kind();
    const bool pinned = scanned_out || kind == AllocationKind::surface || kind == AllocationKind::staging;
    const uint32_t segment = pinned ? ApertureSegment::segment_id
                             : kind == AllocationKind::host_heap
                                 ? segments_.host_heap_segment_id(descriptor_.cpu_visible())
                                 : segments_.system_memory_segment_id();
    info.PreferredSegment = segment;
    info.SupportedReadSegmentSet = 1u << (segment - 1u);
    info.SupportedWriteSegmentSet = 1u << (segment - 1u);
    info.EvictionSegmentSet = 0u;
    info.MaximumRenamingListLength = 0u;
    info.hAllocation = this;
    const bool cached = kind == AllocationKind::staging ||
                        (kind == AllocationKind::cross_adapter && descriptor_.layout().format() != 0u);
    info.Flags = (descriptor_.cpu_visible() ? flag_cpu_visible : 0u) | (cached ? flag_cached : 0u) |
                 (pinned ? flag_accessed_physically : 0u);
    info.AllocationPriority = priority_normal;
    info.Flags2 = 0u;
}

Status Allocation::open(const void *device, const void *process)
{
    AV_SCOPE("Allocation::open");
    platform_.require_irql_at_most(Irql::apc, "Allocation::open");
    InvariantScope<Allocation> guard(platform_, *this, "open");
    if (names_a_context() && owner_ != nullptr && owner_ != device) return Status::invalid_parameter;
    if (opens_ == 0u) {
        owner_ = device;
        process_ = process;
    }
    opens_++;
    return Status::ok;
}

bool Allocation::held_with(const Allocation &other) const
{
    AV_SCOPE("Allocation::held_with");
    return owner_ == other.owner_;
}

bool Allocation::held_in_process_of(const Allocation &other) const
{
    AV_SCOPE("Allocation::held_in_process_of");
    return owner_ != nullptr && other.owner_ != nullptr && process_ == other.process_;
}

Status Allocation::close()
{
    AV_SCOPE("Allocation::close");
    platform_.require_irql_at_most(Irql::apc, "Allocation::close");
    InvariantScope<Allocation> guard(platform_, *this, "close");
    if (opens_ == 0u) return Status::invalid_state;
    opens_--;
    if (opens_ == 0u) {
        owner_ = nullptr;
        process_ = nullptr;
    }
    return Status::ok;
}

Status Allocation::map_aperture(const DxgkBuildPagingBufferMapApertureSegmentRecord &request)
{
    AV_SCOPE("Allocation::map_aperture");
    platform_.require_irql_at_most(Irql::apc, "Allocation::map_aperture");
    return aperture_.map(request);
}

void Allocation::unmap_aperture(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request)
{
    AV_SCOPE("Allocation::unmap_aperture");
    platform_.require_irql_at_most(Irql::apc, "Allocation::unmap_aperture");
    aperture_.unmap(request);
}

GuestPageList Allocation::pages_at(uint64_t segment_address, uint32_t page_count) const
{
    AV_SCOPE("Allocation::pages_at");
    platform_.require_irql_at_most(Irql::dispatch, "Allocation::pages_at");
    const auto base = static_cast<uint64_t>(ApertureSegment::base_address);
    if (segment_address < base) return GuestPageList();
    const uint64_t offset = segment_address - base;
    if ((offset & (page_bytes - 1u)) != 0u) return GuestPageList();
    return aperture_.pages(offset / page_bytes, page_count);
}

GuestPageList Allocation::pages() const
{
    AV_SCOPE("Allocation::pages");
    platform_.require_irql_at_most(Irql::dispatch, "Allocation::pages");
    return aperture_.whole();
}

bool Allocation::names_a_context() const
{
    AV_SCOPE("Allocation::names_a_context");
    return descriptor_.kind() == AllocationKind::context_ring;
}

bool Allocation::invariants_hold() const
{
    AV_SCOPE("Allocation::invariants_hold");
    if ((opens_ == 0u) != (owner_ == nullptr)) return false;
    if (opens_ == 0u && process_ != nullptr) return false;
    return size_ != 0u && (size_ & (page_bytes - 1u)) == 0u && size_ >= descriptor_.size() &&
           aperture_.invariants_hold();
}

}
