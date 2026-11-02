#include "miniport/present/present_page_view.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/pages/present_progress.hpp"

namespace av {

PresentPageView::PresentPageView(Platform &platform) : platform_(platform), page_(nullptr)
{
    AV_SCOPE("PresentPageView::PresentPageView");
}

PresentPageView::~PresentPageView()
{
    AV_SCOPE("PresentPageView::~PresentPageView");
    if (page_ != nullptr) platform_.release_contiguous(page_, PresentProgress::page_size);
}

Status PresentPageView::initialize()
{
    AV_SCOPE("PresentPageView::initialize");
    platform_.require_irql_at_most(Irql::passive, "PresentPageView::initialize");
    if (page_ != nullptr) return Status::invalid_state;
    page_ = static_cast<uint8_t *>(platform_.allocate_contiguous(PresentProgress::page_size));
    if (page_ == nullptr) return Status::no_memory;
    clear();
    return Status::ok;
}

void PresentPageView::clear()
{
    AV_SCOPE("PresentPageView::clear");
    platform_.require_irql_at_most(Irql::passive, "PresentPageView::clear");
    if (page_ != nullptr) __builtin_memset(page_, 0, PresentProgress::page_size);
}

uint64_t PresentPageView::address() const
{
    AV_SCOPE("PresentPageView::address");
    return page_ != nullptr ? platform_.physical_address(page_) : 0u;
}

uint32_t *PresentPageView::word(uint64_t offset) const
{
    AV_SCOPE("PresentPageView::word");
    return reinterpret_cast<uint32_t *>(page_ + offset);
}

Status PresentPageView::read(PresentProgress &progress) const
{
    AV_SCOPE("PresentPageView::read");
    platform_.require_irql_at_most(Irql::dispatch, "PresentPageView::read");
    if (page_ == nullptr) return Status::invalid_state;
    uint8_t        bytes[PresentProgress::encoded_size] = {};
    const uint64_t refused =
        __atomic_load_n(reinterpret_cast<uint64_t *>(page_ + PresentProgress::refused_at), __ATOMIC_ACQUIRE);
    const uint64_t completed =
        __atomic_load_n(reinterpret_cast<uint64_t *>(page_ + PresentProgress::completed_at), __ATOMIC_ACQUIRE);
    for (uint64_t at = 0u; at < PresentProgress::completed_at; at += sizeof(uint32_t)) {
        const uint32_t value = __atomic_load_n(word(at), __ATOMIC_RELAXED);
        __builtin_memcpy(bytes + at, &value, sizeof value);
    }
    __builtin_memcpy(bytes + PresentProgress::completed_at, &completed, sizeof completed);
    __builtin_memcpy(bytes + PresentProgress::refused_at, &refused, sizeof refused);
    return progress.decode(bytes, sizeof bytes);
}

}
