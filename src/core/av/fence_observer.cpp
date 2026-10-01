#include "core/av/fence_observer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/fence_progress.hpp"

namespace av {

FenceObserver::FenceObserver(Platform &platform, FenceProgress &fences)
    : platform_(platform), fences_(fences), page_(nullptr), fault_code_(0u), failed_(false)
    {
        AV_SCOPE("FenceObserver::FenceObserver");
    }

Status FenceObserver::attach(const uint8_t *page)
{
    AV_SCOPE("FenceObserver::attach");
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
    AV_SCOPE("FenceObserver::detach");
    platform_.require_irql_at_most(Irql::dispatch, "FenceObserver::detach");
    page_ = nullptr;
    failed_ = false;
}

Status FenceObserver::observe()
{
    AV_SCOPE("FenceObserver::observe");
    platform_.require_irql_at_most(Irql::isr, "FenceObserver::observe");
    if (page_ == nullptr || failed_) return Status::invalid_state;
    uint8_t bytes[FencePage::encoded_size] = {};
    platform_.copy(bytes, page_, sizeof bytes);
    FencePage page;
    if (page.decode(bytes, sizeof bytes) != Status::ok) return fail("av: the device wrote a malformed fence page");
    if (fences_.observe(page) != Status::ok) return fail("av: the device's fence page contradicts what was seen");
    fault_code_ = page.fault_code();
    return Status::ok;
}

Status FenceObserver::fail(const char *reason)
{
    AV_SCOPE("FenceObserver::fail");
    platform_.log(reason);
    failed_ = true;
    return Status::invalid_state;
}

bool     FenceObserver::attached() const
{
    AV_SCOPE("FenceObserver::attached");
    return page_ != nullptr;
}
uint64_t FenceObserver::completed() const
{
    AV_SCOPE("FenceObserver::completed");
    return fences_.completed_fence();
}
uint64_t FenceObserver::progress() const
{
    AV_SCOPE("FenceObserver::progress");
    return fences_.progress();
}
bool     FenceObserver::faulted() const
{
    AV_SCOPE("FenceObserver::faulted");
    return fences_.faulted();
}
uint32_t FenceObserver::fault_code() const
{
    AV_SCOPE("FenceObserver::fault_code");
    return fault_code_;
}
bool     FenceObserver::failed() const
{
    AV_SCOPE("FenceObserver::failed");
    return failed_;
}

/* Only an attached page can have been judged a lie. */
bool FenceObserver::invariants_hold() const
{
    AV_SCOPE("FenceObserver::invariants_hold");
    return page_ != nullptr || !failed_;
}

}  // namespace av
