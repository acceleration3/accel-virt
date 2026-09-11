#include "core/rvk/fence_observer.hpp"

#include "proto/submission/fence_page.hpp"
#include "proto/submission/fence_progress.hpp"

namespace av {

FenceObserver::FenceObserver(Platform &platform, FenceProgress &fences)
    : platform_(platform), fences_(fences), page_(nullptr), fault_code_(0u), failed_(false) {}

Status FenceObserver::attach(const uint8_t *page)
{
    platform_.require_irql_at_most(Irql::dispatch, "FenceObserver::attach");
    if (page_ != nullptr) return Status::invalid_state;
    if (page == nullptr) return Status::invalid_parameter;
    page_ = page;
    fault_code_ = 0u;
    failed_ = false;
    return Status::ok;
}

void FenceObserver::detach()
{
    platform_.require_irql_at_most(Irql::dispatch, "FenceObserver::detach");
    page_ = nullptr;
    failed_ = false;
}

Status FenceObserver::observe()
{
    platform_.require_irql_at_most(Irql::isr, "FenceObserver::observe");
    if (page_ == nullptr || failed_) return Status::invalid_state;
    uint8_t bytes[FencePage::encoded_size] = {};
    platform_.copy(bytes, page_, sizeof bytes);
    FencePage page;
    if (page.decode(bytes, sizeof bytes) != Status::ok) return fail("rvk: the device wrote a malformed fence page");
    if (fences_.observe(page) != Status::ok) return fail("rvk: the device's fence page contradicts what was seen");
    fault_code_ = page.fault_code();
    return Status::ok;
}

Status FenceObserver::fail(const char *reason)
{
    platform_.log(reason);
    failed_ = true;
    return Status::invalid_state;
}

bool     FenceObserver::attached() const   { return page_ != nullptr; }
uint64_t FenceObserver::completed() const  { return fences_.completed_fence(); }
uint64_t FenceObserver::progress() const   { return fences_.progress(); }
bool     FenceObserver::faulted() const    { return fences_.faulted(); }
uint32_t FenceObserver::fault_code() const { return fault_code_; }
bool     FenceObserver::failed() const     { return failed_; }

/* Only an attached page can have been judged a lie. */
bool FenceObserver::invariants_hold() const
{
    return page_ != nullptr || !failed_;
}

}  // namespace av
