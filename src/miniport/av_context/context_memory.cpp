#include "miniport/av_context/context_memory.hpp"

#include "core/platform/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

namespace {

constexpr uint64_t page_bytes = GuestPageList::page_bytes;
constexpr uint64_t page_mask = page_bytes - 1u;

}

ContextMemory::ContextMemory(Platform &platform)
    : platform_(platform), ring_(platform), fence_(platform), reply_(platform), ring_header_(nullptr),
      fence_view_(nullptr), order_(0u)
{
    AV_SCOPE("ContextMemory::ContextMemory");
}

ContextMemory::~ContextMemory()
{
    AV_SCOPE("ContextMemory::~ContextMemory");
    release();
}

Status ContextMemory::allocate(uint8_t order, uint32_t reply_bytes)
{
    AV_SCOPE("ContextMemory::allocate");
    platform_.require_irql_at_most(Irql::passive, "ContextMemory::allocate");
    InvariantScope<ContextMemory> guard(platform_, *this, "allocate");
    if (ring_.held()) return Status::invalid_state;
    if (order < RingHeader::lowest_order || order > RingHeader::highest_order || reply_bytes == 0u ||
        reply_bytes > AvContextRecord::max_reply_bytes)
        return Status::invalid_parameter;
    ScopeExit      release_all([this] { free_all(); });
    const uint64_t ring_bytes = (RingHeader::encoded_size + (uint64_t{1} << order) + page_mask) & ~page_mask;
    Status         status = ring_.allocate(ring_bytes);
    if (status == Status::ok) status = fence_.allocate(page_bytes);
    if (status == Status::ok) status = reply_.allocate((uint64_t{reply_bytes} + page_mask) & ~page_mask);
    if (status != Status::ok) return Status::no_memory;
    ring_header_ = platform_.map_frame(ring_.pages().frame(0u));
    fence_view_ = platform_.map_frame(fence_.pages().frame(0u));
    if (ring_header_ == nullptr || fence_view_ == nullptr) return Status::no_memory;
    release_all.dismiss();
    order_ = order;
    static_cast<void>(RingHeader(order, 0u, 0u).encode(ring_header_, RingHeader::encoded_size));
    return Status::ok;
}

Status ContextMemory::map_into_process()
{
    AV_SCOPE("ContextMemory::map_into_process");
    platform_.require_irql_at_most(Irql::passive, "ContextMemory::map_into_process");
    InvariantScope<ContextMemory> guard(platform_, *this, "map_into_process");
    if (!ring_.held() || ring_.view() != 0u) return Status::invalid_state;
    const Status ring = ring_.map_into_process();
    if (ring != Status::ok) return ring;
    ScopeExit    unmap_all([this] {
        reply_.unmap_from_process();
        ring_.unmap_from_process();
    });
    const Status reply = reply_.map_into_process();
    if (reply != Status::ok) return reply;
    const Status fence = fence_.map_read_only_into_process();
    if (fence != Status::ok) return fence;
    unmap_all.dismiss();
    return Status::ok;
}

void ContextMemory::unmap_kernel_views()
{
    AV_SCOPE("ContextMemory::unmap_kernel_views");
    if (fence_view_ != nullptr) platform_.unmap_frame(fence_view_);
    if (ring_header_ != nullptr) platform_.unmap_frame(ring_header_);
    fence_view_ = nullptr;
    ring_header_ = nullptr;
}

void ContextMemory::release()
{
    AV_SCOPE("ContextMemory::release");
    platform_.require_irql_at_most(Irql::passive, "ContextMemory::release");
    InvariantScope<ContextMemory> guard(platform_, *this, "release");
    free_all();
}

void ContextMemory::free_all()
{
    AV_SCOPE("ContextMemory::free_all");
    unmap_kernel_views();
    reply_.release();
    fence_.release();
    ring_.release();
    order_ = 0u;
}

void ContextMemory::abandon()
{
    AV_SCOPE("ContextMemory::abandon");
    platform_.require_irql_at_most(Irql::passive, "ContextMemory::abandon");
    InvariantScope<ContextMemory> guard(platform_, *this, "abandon");
    unmap_kernel_views();
    reply_.abandon();
    fence_.abandon();
    ring_.abandon();
    order_ = 0u;
}

bool ContextMemory::allocated() const
{
    AV_SCOPE("ContextMemory::allocated");
    return ring_.held();
}

uint8_t ContextMemory::order() const
{
    AV_SCOPE("ContextMemory::order");
    return order_;
}

uint64_t ContextMemory::ring_view() const
{
    AV_SCOPE("ContextMemory::ring_view");
    return ring_.view();
}

uint64_t ContextMemory::reply_view() const
{
    AV_SCOPE("ContextMemory::reply_view");
    return reply_.view();
}

uint64_t ContextMemory::fence_view() const
{
    AV_SCOPE("ContextMemory::fence_view");
    return fence_.view();
}

ContextPages ContextMemory::pages() const
{
    AV_SCOPE("ContextMemory::pages");
    if (!ring_.held()) return ContextPages();
    return ContextPages(ring_.pages(), fence_.pages(), reply_.pages(), ring_header_, fence_view_);
}

bool ContextMemory::invariants_hold() const
{
    AV_SCOPE("ContextMemory::invariants_hold");
    if (!ring_.invariants_hold() || !fence_.invariants_hold() || !reply_.invariants_hold()) return false;
    const bool held = ring_.held();
    if (fence_.held() != held || reply_.held() != held || (order_ != 0u) != held) return false;
    if ((ring_header_ != nullptr) != held || (fence_view_ != nullptr) != held) return false;
    return (ring_.view() != 0u) == (reply_.view() != 0u) && (ring_.view() != 0u) == (fence_.view() != 0u);
}

}
