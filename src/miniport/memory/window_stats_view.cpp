#include "miniport/memory/window_stats_view.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/pages/window_stats.hpp"

namespace av {

WindowStatsView::WindowStatsView(Platform &platform) : platform_(platform), page_(nullptr)
{
    AV_SCOPE("WindowStatsView::WindowStatsView");
}

WindowStatsView::~WindowStatsView()
{
    AV_SCOPE("WindowStatsView::~WindowStatsView");
    if (page_ != nullptr) platform_.release_contiguous(page_, WindowStats::page_size);
}

Status WindowStatsView::initialize()
{
    AV_SCOPE("WindowStatsView::initialize");
    platform_.require_irql_at_most(Irql::passive, "WindowStatsView::initialize");
    if (page_ != nullptr) return Status::ok;
    page_ = static_cast<uint8_t *>(platform_.allocate_contiguous(WindowStats::page_size));
    if (page_ == nullptr) return Status::no_memory;
    __builtin_memset(page_, 0, WindowStats::page_size);
    uint8_t      header[WindowStats::encoded_size] = {};
    const Status encoded = WindowStats().encode(header, sizeof header);
    if (encoded != Status::ok) return encoded;
    __builtin_memcpy(page_, header, WindowStats::fields_at);
    return Status::ok;
}

bool WindowStatsView::ready() const
{
    AV_SCOPE("WindowStatsView::ready");
    return page_ != nullptr;
}

uint64_t WindowStatsView::address() const
{
    AV_SCOPE("WindowStatsView::address");
    return page_ != nullptr ? platform_.physical_address(page_) : 0u;
}

uint32_t *WindowStatsView::sequence() const
{
    AV_SCOPE("WindowStatsView::sequence");
    return reinterpret_cast<uint32_t *>(page_ + WindowStats::sequence_at);
}

uint64_t *WindowStatsView::field(uint32_t index) const
{
    AV_SCOPE("WindowStatsView::field");
    return reinterpret_cast<uint64_t *>(page_ + WindowStats::fields_at + uint64_t{index} * sizeof(uint64_t));
}

bool WindowStatsView::invariants_hold() const
{
    AV_SCOPE("WindowStatsView::invariants_hold");
    return page_ == nullptr || (__atomic_load_n(sequence(), __ATOMIC_RELAXED) & 1u) == 0u;
}

void WindowStatsView::publish(const WindowStats &stats)
{
    AV_SCOPE("WindowStatsView::publish");
    platform_.require_irql_at_most(Irql::passive, "WindowStatsView::publish");
    if (page_ == nullptr) return;
    const uint32_t before = __atomic_load_n(sequence(), __ATOMIC_RELAXED);
    __atomic_store_n(sequence(), before + 1u, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    for (uint32_t index = 0u; index < WindowStats::field_count; index++)
        __atomic_store_n(field(index), stats.field(index), __ATOMIC_RELAXED);
    __atomic_store_n(sequence(), before + 2u, __ATOMIC_RELEASE);
}

}
