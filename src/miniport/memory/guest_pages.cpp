#include "miniport/memory/guest_pages.hpp"

#include "core/platform/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

GuestPages::GuestPages(Platform &platform)
    : platform_(platform), record_(nullptr), frames_(nullptr), bytes_(0u), mapping_(nullptr), view_(0u)
{
    AV_SCOPE("GuestPages::GuestPages");
}

GuestPages::~GuestPages()
{
    AV_SCOPE("GuestPages::~GuestPages");
    release();
}

Status GuestPages::allocate(uint64_t bytes)
{
    AV_SCOPE("GuestPages::allocate");
    platform_.require_irql_at_most(Irql::passive, "GuestPages::allocate");
    InvariantScope<GuestPages> guard(platform_, *this, "allocate");
    if (record_ != nullptr) return Status::invalid_state;
    const uint64_t page_bytes = GuestPageList::page_bytes;
    if (bytes == 0u || bytes % page_bytes != 0u || bytes / page_bytes > UINT32_MAX) return Status::invalid_parameter;
    const auto count = static_cast<uint32_t>(bytes / page_bytes);
    void *const record = platform_.allocate_pages(bytes);
    if (record == nullptr) return Status::no_memory;
    ScopeExit free_pages([this, record] { platform_.release_pages(record); });
    auto *const frames = static_cast<uint64_t *>(platform_.allocate(size_t{count} * sizeof *frames_, Pool::non_paged));
    if (frames == nullptr) return Status::no_memory;
    static_cast<void>(platform_.read_page_frames(record, 0u, count, frames));
    free_pages.dismiss();
    record_ = record;
    frames_ = frames;
    bytes_ = bytes;
    return Status::ok;
}

Status GuestPages::map_into_process()
{
    AV_SCOPE("GuestPages::map_into_process");
    platform_.require_irql_at_most(Irql::passive, "GuestPages::map_into_process");
    InvariantScope<GuestPages> guard(platform_, *this, "map_into_process");
    if (record_ == nullptr || mapping_ != nullptr) return Status::invalid_state;
    return platform_.map_pages_to_process(record_, view_, mapping_);
}

Status GuestPages::map_read_only_into_process()
{
    AV_SCOPE("GuestPages::map_read_only_into_process");
    platform_.require_irql_at_most(Irql::passive, "GuestPages::map_read_only_into_process");
    InvariantScope<GuestPages> guard(platform_, *this, "map_read_only_into_process");
    if (record_ == nullptr || mapping_ != nullptr) return Status::invalid_state;
    return platform_.map_pages_to_process_read_only(record_, view_, mapping_);
}

void GuestPages::unmap_from_process()
{
    AV_SCOPE("GuestPages::unmap_from_process");
    platform_.require_irql_at_most(Irql::passive, "GuestPages::unmap_from_process");
    if (mapping_ != nullptr) platform_.unmap_from_process(mapping_);
    mapping_ = nullptr;
    view_ = 0u;
}

void GuestPages::release()
{
    AV_SCOPE("GuestPages::release");
    platform_.require_irql_at_most(Irql::passive, "GuestPages::release");
    InvariantScope<GuestPages> guard(platform_, *this, "release");
    unmap_from_process();
    if (record_ != nullptr) platform_.release_pages(record_);
    if (frames_ != nullptr) platform_.release(frames_);
    record_ = nullptr;
    frames_ = nullptr;
    bytes_ = 0u;
}

void GuestPages::abandon()
{
    AV_SCOPE("GuestPages::abandon");
    platform_.require_irql_at_most(Irql::passive, "GuestPages::abandon");
    InvariantScope<GuestPages> guard(platform_, *this, "abandon");
    unmap_from_process();
    if (frames_ != nullptr) platform_.release(frames_);
    record_ = nullptr;
    frames_ = nullptr;
    bytes_ = 0u;
}

bool GuestPages::held() const
{
    AV_SCOPE("GuestPages::held");
    return record_ != nullptr;
}

uint64_t GuestPages::bytes() const
{
    AV_SCOPE("GuestPages::bytes");
    return bytes_;
}

GuestPageList GuestPages::pages() const
{
    AV_SCOPE("GuestPages::pages");
    if (record_ == nullptr) return GuestPageList();
    return GuestPageList(frames_, static_cast<uint32_t>(bytes_ / GuestPageList::page_bytes));
}

uint64_t GuestPages::view() const
{
    AV_SCOPE("GuestPages::view");
    return view_;
}

bool GuestPages::invariants_hold() const
{
    AV_SCOPE("GuestPages::invariants_hold");
    const bool held = record_ != nullptr;
    if ((frames_ != nullptr) != held || (bytes_ != 0u) != held) return false;
    if ((view_ != 0u) != (mapping_ != nullptr)) return false;
    return held || mapping_ == nullptr;
}

}
