#include "device/fence/fence_reporter.hpp"

#include "core/trace/function_scope.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/host/host.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/threading/doorbell.hpp"
#include "proto/submission/fence_page.hpp"

namespace av {

FenceReporter::FenceReporter(Host &host, FenceInterrupts &interrupts, WireRecorder &recorder,
                             uint32_t context)
    : host_(host), interrupts_(interrupts), recorder_(recorder), context_(context), page_(), pulse_(nullptr), line_(0u), completed_(0u), progress_(0u),
      raised_(0u), beats_(0u), fault_(FaultCode::none), prepared_(false), faulted_(false), pending_(false),
      working_(false), quiet_(true)
      {
          AV_SCOPE("FenceReporter::FenceReporter");
      }

Status FenceReporter::prepare(const FenceWindow &page, uint32_t line)
{
    AV_SCOPE("FenceReporter::prepare");
    host_.require_main_loop("FenceReporter::prepare");
    if (prepared_) return Status::invalid_state;
    if (!page.invariants_hold() || line >= Doorbell::pages) return Status::invalid_parameter;
    static_cast<void>(page.publish(FencePage(0u, 0u)));
    static_cast<void>(page.publish_beat(0u));
    page_ = page;
    line_ = line;
    completed_ = 0u;
    progress_ = 0u;
    raised_ = 0u;
    beats_ = 0u;
    prepared_ = true;
    __atomic_store_n(&quiet_, false, __ATOMIC_RELEASE);
    return Status::ok;
}

void FenceReporter::withdraw()
{
    AV_SCOPE("FenceReporter::withdraw");
    host_.require_main_loop("FenceReporter::withdraw");
    page_ = FenceWindow();
    prepared_ = false;
    __atomic_store_n(&quiet_, true, __ATOMIC_RELEASE);
    faulted_ = false;
    fault_ = FaultCode::none;
    pending_ = false;
}

void FenceReporter::complete(uint64_t fence)
{
    AV_SCOPE("FenceReporter::complete");
    host_.require_worker("FenceReporter::complete");
    if (!prepared_ || faulted_) return;
    completed_ = fence;
    progress_++;
    if (pulse_ != nullptr) pulse_->note_completion();
    pending_ = true;
    static_cast<void>(page_.publish(FencePage(completed_, progress_).raising(raised_)));
    recorder_.record_fence(context_, fence);
}

void FenceReporter::heartbeat()
{
    AV_SCOPE("FenceReporter::heartbeat");
    host_.require_worker("FenceReporter::heartbeat");
    if (!prepared_ || faulted_) return;
    progress_++;
    static_cast<void>(page_.publish(FencePage(completed_, progress_).raising(raised_)));
}

void FenceReporter::raise()
{
    AV_SCOPE("FenceReporter::raise");
    host_.require_worker("FenceReporter::raise");
    if (!prepared_ || faulted_) return;
    progress_++;
    raised_++;
    pending_ = true;
    static_cast<void>(page_.publish(FencePage(completed_, progress_).raising(raised_)));
}

void FenceReporter::fault(FaultCode code)
{
    AV_SCOPE("FenceReporter::fault");
    host_.require_worker("FenceReporter::fault");
    if (!prepared_ || faulted_) return;
    faulted_ = true;
    fault_ = code;
    pending_ = true;
    __atomic_store_n(&quiet_, true, __ATOMIC_RELEASE);
    __atomic_store_n(&working_, false, __ATOMIC_RELEASE);
    static_cast<void>(page_.publish(FencePage(completed_, progress_, static_cast<uint32_t>(code)).raising(raised_)));
}

void FenceReporter::signal()
{
    AV_SCOPE("FenceReporter::signal");
    host_.require_worker("FenceReporter::signal");
    if (!pending_) return;
    pending_ = false;
    interrupts_.raise(line_);
}

void FenceReporter::at_work(bool working)
{
    AV_SCOPE("FenceReporter::at_work");
    __atomic_store_n(&working_, working, __ATOMIC_SEQ_CST);
    if (working && pulse_ != nullptr) pulse_->stir();
}

void FenceReporter::beat_by(DevicePulse &pulse)
{
    AV_SCOPE("FenceReporter::beat_by");
    pulse_ = &pulse;
}

bool FenceReporter::working() const
{
    AV_SCOPE("FenceReporter::working");
    return __atomic_load_n(&working_, __ATOMIC_SEQ_CST);
}

void FenceReporter::beat()
{
    AV_SCOPE("FenceReporter::beat");
    if (__atomic_load_n(&quiet_, __ATOMIC_ACQUIRE)) return;
    beats_++;
    static_cast<void>(page_.publish_beat(beats_));
}

uint64_t FenceReporter::beats() const
{
    AV_SCOPE("FenceReporter::beats");
    return beats_;
}

uint64_t FenceReporter::device_completions() const
{
    AV_SCOPE("FenceReporter::device_completions");
    return pulse_ != nullptr ? pulse_->completions() : 0u;
}

bool     FenceReporter::faulted() const
{
    AV_SCOPE("FenceReporter::faulted");
    return faulted_;
}
uint64_t FenceReporter::completed() const
{
    AV_SCOPE("FenceReporter::completed");
    return completed_;
}
uint64_t FenceReporter::progress() const
{
    AV_SCOPE("FenceReporter::progress");
    return progress_;
}
uint64_t FenceReporter::raised() const
{
    AV_SCOPE("FenceReporter::raised");
    return raised_;
}

bool FenceReporter::invariants_hold() const
{
    AV_SCOPE("FenceReporter::invariants_hold");
    const bool quiet = __atomic_load_n(&quiet_, __ATOMIC_ACQUIRE);
    if (!prepared_) return !pending_ && !faulted_ && quiet;
    return page_.invariants_hold() && (completed_ == 0u || progress_ != 0u) && raised_ <= progress_ &&
           (!faulted_ || (fault_ != FaultCode::none && quiet));
}

}
