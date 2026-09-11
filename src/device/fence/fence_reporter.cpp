#include "device/fence/fence_reporter.hpp"

#include "device/fence/fence_interrupts.hpp"
#include "device/host/host.hpp"
#include "device/threading/doorbell.hpp"
#include "proto/submission/fence_page.hpp"

namespace av {

FenceReporter::FenceReporter(Host &host, FenceInterrupts &interrupts)
    : host_(host), interrupts_(interrupts), page_(), line_(0u), completed_(0u), progress_(0u), prepared_(false),
      pending_(false) {}

Status FenceReporter::prepare(const FenceWindow &page, uint32_t line)
{
    host_.require_main_loop("FenceReporter::prepare");
    if (prepared_) return Status::invalid_state;
    if (!page.invariants_hold() || line >= Doorbell::pages) return Status::invalid_parameter;
    static_cast<void>(page.publish(FencePage(0u, 0u)));   /* the window holds a page */
    page_ = page;
    line_ = line;
    completed_ = 0u;
    progress_ = 0u;
    prepared_ = true;
    return Status::ok;
}

void FenceReporter::withdraw()
{
    host_.require_main_loop("FenceReporter::withdraw");
    page_ = FenceWindow();
    prepared_ = false;
}

void FenceReporter::complete(uint64_t fence)
{
    host_.require_worker("FenceReporter::complete");
    if (!prepared_) return;
    completed_ = fence;
    progress_++;
    pending_ = true;
    static_cast<void>(page_.publish(FencePage(completed_, progress_)));   /* checked when prepared */
}

void FenceReporter::signal()
{
    host_.require_worker("FenceReporter::signal");
    if (!pending_) return;
    pending_ = false;
    interrupts_.raise(line_);
}

uint64_t FenceReporter::completed() const { return completed_; }
uint64_t FenceReporter::progress() const  { return progress_; }

/* Nothing completes before the page exists, and every completion moved progress. */
bool FenceReporter::invariants_hold() const
{
    if (!prepared_) return !pending_;
    return page_.invariants_hold() && (completed_ == 0u || progress_ != 0u);
}

}  // namespace av
