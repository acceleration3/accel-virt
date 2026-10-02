#include "miniport/memory/aperture_pages.hpp"

#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

AperturePages::AperturePages(Platform &platform, uint32_t capacity)
    : platform_(platform), capacity_(capacity), frames_(nullptr), base_page_(0u)
    {
        AV_SCOPE("AperturePages::AperturePages");
    }

AperturePages::~AperturePages()
{
    AV_SCOPE("AperturePages::~AperturePages");
    if (frames_ != nullptr) platform_.release(frames_);
}

void AperturePages::forget_all()
{
    AV_SCOPE("AperturePages::forget_all");
    platform_.zero(frames_, capacity_ * sizeof *frames_);
}

Status AperturePages::map(const DxgkBuildPagingBufferMapApertureSegmentRecord &request)
{
    AV_SCOPE("AperturePages::map");
    platform_.require_irql_at_most(Irql::passive, "AperturePages::map");
    InvariantScope<AperturePages> guard(platform_, *this, "map");
    if (request.NumberOfPages == 0u || request.NumberOfPages > capacity_) return Status::invalid_parameter;
    if (request.OffsetInPages > ~uint64_t{0} - request.NumberOfPages) return Status::invalid_parameter;
    const uint32_t count = static_cast<uint32_t>(request.NumberOfPages);
    if (frames_ == nullptr) {
        frames_ = static_cast<uint64_t *>(platform_.allocate(capacity_ * sizeof *frames_, Pool::non_paged));
        if (frames_ == nullptr) return Status::no_memory;
        base_page_ = request.OffsetInPages;
        forget_all();
    }
    if (!holds(request.OffsetInPages, count)) {
        base_page_ = request.OffsetInPages;
        forget_all();
    }
    const uint32_t first = static_cast<uint32_t>(request.OffsetInPages - base_page_);
    return platform_.read_page_frames(request.pMdl, request.MdlOffset, count, frames_ + first);
}

void AperturePages::unmap(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request)
{
    AV_SCOPE("AperturePages::unmap");
    platform_.require_irql_at_most(Irql::passive, "AperturePages::unmap");
    InvariantScope<AperturePages> guard(platform_, *this, "unmap");
    if (frames_ == nullptr || request.NumberOfPages == 0u) return;
    const uint64_t end = request.OffsetInPages + request.NumberOfPages;
    const uint64_t taken_end = end < request.OffsetInPages ? ~uint64_t{0} : end;
    const uint64_t window_end = base_page_ + capacity_;
    const uint64_t first = request.OffsetInPages > base_page_ ? request.OffsetInPages : base_page_;
    const uint64_t last = taken_end < window_end ? taken_end : window_end;
    if (first >= last) return;
    platform_.zero(frames_ + (first - base_page_), (last - first) * sizeof *frames_);
}

bool AperturePages::holds(uint64_t first_page, uint32_t count) const
{
    AV_SCOPE("AperturePages::holds");
    return first_page >= base_page_ && first_page - base_page_ <= capacity_ - count;
}

GuestPageList AperturePages::pages(uint64_t first_page, uint32_t count) const
{
    AV_SCOPE("AperturePages::pages");
    platform_.require_irql_at_most(Irql::dispatch, "AperturePages::pages");
    if (frames_ == nullptr || count == 0u || count > capacity_) return GuestPageList();
    if (!holds(first_page, count)) return GuestPageList();
    const GuestPageList list(frames_ + (first_page - base_page_), count);
    return list.complete() ? list : GuestPageList();
}

GuestPageList AperturePages::whole() const
{
    AV_SCOPE("AperturePages::whole");
    platform_.require_irql_at_most(Irql::dispatch, "AperturePages::whole");
    return pages(base_page_, capacity_);
}

bool AperturePages::invariants_hold() const
{
    AV_SCOPE("AperturePages::invariants_hold");
    return capacity_ != 0u && (frames_ != nullptr || base_page_ == 0u);
}

}  // namespace av
