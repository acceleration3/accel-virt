#include "device/context/ring_consumer.hpp"

#include "device/context/d3d_ordering.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/env_setting.hpp"
#include "device/execution/batch_executor.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "core/support/text_buffer.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/ring_header.hpp"

#include <stdio.h>
#include <time.h>

namespace av {

namespace {

uint64_t watchdog_limit()
{
    AV_SCOPE("watchdog_limit");
    uint64_t milliseconds = 0u;
    if (!EnvSetting("AV_SUBMIT_WATCHDOG_MS").number(milliseconds)) return SubmissionWatchdog::default_limit_ns;
    return milliseconds * 1000000u;
}

}

RingConsumer::RingConsumer(Host &host, FenceReporter &fences, BatchExecutor &batches)
    : host_(host), fences_(fences), executor_(batches), ring_(), progress_(), held_(), watchdog_(watchdog_limit()), head_(0u), running_end_(0u), last_fence_(0u), batches_(0u), ordering_(nullptr), clock_pairs_(EnvSetting("AV_PERFETTO").on()), prepared_(false),
      attached_(false), faulted_(false), serving_(false), publishing_(false)
      {
          AV_SCOPE("RingConsumer::RingConsumer");
      }

Status RingConsumer::prepare(const RingWindow &ring)
{
    AV_SCOPE("RingConsumer::prepare");
    host_.require_main_loop("RingConsumer::prepare");
    if (prepared_) return Status::invalid_state;
    RingHeader header;
    if (!ring.invariants_hold() || ring.snapshot(header) != Status::ok || header.order() != ring.order() ||
        header.head() != 0u)
        return Status::invalid_parameter;
    RingProgress progress;
    if (progress.observe(header) != Status::ok) return Status::invalid_parameter;
    ring_ = ring;
    progress_ = progress;
    prepared_ = true;
    return Status::ok;
}

void RingConsumer::withdraw()
{
    AV_SCOPE("RingConsumer::withdraw");
    host_.require_main_loop("RingConsumer::withdraw");
    ring_ = RingWindow();
    progress_ = RingProgress();
    prepared_ = false;
}

void RingConsumer::attach()
{
    AV_SCOPE("RingConsumer::attach");
    host_.require_worker("RingConsumer::attach");
    attached_ = prepared_;
    if (!attached_ || executor_.start() == Status::ok) return;
    fault(FaultCode::no_executor, "the context's executor could not be created", 0u);
    fences_.signal();
}

void RingConsumer::abandon()
{
    AV_SCOPE("RingConsumer::abandon");
    executor_.abandon();
}

void RingConsumer::detach()
{
    AV_SCOPE("RingConsumer::detach");
    host_.require_worker("RingConsumer::detach");
    executor_.stop();
}

void RingConsumer::service()
{
    AV_SCOPE("RingConsumer::service");
    host_.require_worker("RingConsumer::service");
    if (!attached_ || faulted_) return;
    serve();
    static_cast<void>(settle());
    fences_.signal();
}

void RingConsumer::serve()
{
    AV_SCOPE("RingConsumer::serve");
    RingHeader snapshot;
    if (ring_.snapshot(snapshot) != Status::ok)
        return fault(FaultCode::ring_header, "ring header unreadable or malformed, order", ring_.order());
    if (snapshot.order() != ring_.order())
        return fault(FaultCode::ring_header, "ring order changed to", snapshot.order());
    if (progress_.observe(RingHeader(snapshot.order(), snapshot.tail(), head_)) != Status::ok)
        return fault(FaultCode::ring_tail, "ring tail inconsistent with what was consumed:", snapshot.tail());
    while (!executor_.abandoned() && consume_next() == Status::ok) {}
}

Status RingConsumer::consume_next()
{
    AV_SCOPE("RingConsumer::consume_next");
    const uint64_t published = progress_.tail() - head_;
    if (published < BatchHeader::encoded_size) return Status::no_space;
    uint8_t     bytes[BatchHeader::encoded_size] = {};
    BatchHeader batch;
    if (ring_.copy(head_, bytes, sizeof bytes) != Status::ok || batch.decode(bytes, sizeof bytes) != Status::ok) {
        fault(FaultCode::batch_header, "batch header unreadable or malformed at head", head_);
        return Status::malformed;
    }
    if (!batch.fits(ring_.capacity())) {
        fault(FaultCode::batch_header, "batch cannot fit the ring, payload bytes", batch.length());
        return Status::malformed;
    }
    if (batch.fence_id() <= last_fence_) {
        fault(FaultCode::batch_order, "batch fence does not follow the last, fence", batch.fence_id());
        return Status::malformed;
    }
    if (published < batch.span()) return Status::no_space;
    serving_ = true;
    note_work();
    ScopeExit served([this] {
        serving_ = false;
        note_work();
    });
    if (ordering_ != nullptr) ordering_->wait_before(*this, batch.published_at());
    note_clock(batch.published_at());
    __atomic_store_n(&running_end_, head_ + batch.span(), __ATOMIC_RELEASE);
    const Status ran = executor_.run(ring_, head_, batch);
    __atomic_store_n(&running_end_, 0u, __ATOMIC_RELEASE);
    if (ran != Status::ok) {
        if (!executor_.abandoned()) fault(executor_.fault(), "the batch could not run, fence", batch.fence_id());
        return ran;
    }
    __atomic_store_n(&head_, head_ + batch.span(), __ATOMIC_RELEASE);
    last_fence_ = batch.fence_id();
    batches_++;
    static_cast<void>(ring_.publish_head(head_));
    complete_or_hold(last_fence_);
    return Status::ok;
}

void RingConsumer::complete_or_hold(uint64_t fence)
{
    AV_SCOPE("RingConsumer::complete_or_hold");
    uint64_t marked = 0u;
    uint64_t reached = 0u;
    executor_.gpu_progress(marked, reached);
    if (held_.empty() && marked <= reached) {
        fences_.complete(fence);
        return;
    }
    while (!held_.hold(fence, marked)) {
        static_cast<void>(settle());
        __builtin_ia32_pause();
    }
    static_cast<void>(settle());
}

bool RingConsumer::settle()
{
    AV_SCOPE("RingConsumer::settle");
    if (held_.empty()) return false;
    static_cast<void>(executor_.poll());
    uint64_t marked = 0u;
    uint64_t reached = 0u;
    executor_.gpu_progress(marked, reached);
    const uint64_t newest = held_.release(reached);
    if (newest != 0u) fences_.complete(newest);
    if (held_.empty()) {
        watchdog_.nothing_outstanding();
        return newest != 0u;
    }
    const uint64_t now = host_.monotonic_nanoseconds();
    watchdog_.waiting_at(reached, now);
    if (watchdog_.overran(now)) {
        fault(FaultCode::device_lost, "a submission outran the watchdog; last fence", last_fence_);
        fences_.signal();
    }
    return newest != 0u;
}

uint64_t RingConsumer::watchdog_deadline() const
{
    AV_SCOPE("RingConsumer::watchdog_deadline");
    return attached_ && !faulted_ ? watchdog_.deadline() : 0u;
}

bool RingConsumer::has_work() const
{
    AV_SCOPE("RingConsumer::has_work");
    if (!attached_ || faulted_) return false;
    RingHeader header;
    return ring_.snapshot(header) == Status::ok &&
           header.tail() - __atomic_load_n(&head_, __ATOMIC_ACQUIRE) >= BatchHeader::encoded_size;
}

bool RingConsumer::poll()
{
    AV_SCOPE("RingConsumer::poll");
    if (!attached_ || faulted_) return false;
    const HostPoll polled = executor_.poll();
    if (polled == HostPoll::lost) {
        fault(FaultCode::device_lost, "the host device was lost; last fence", last_fence_);
        fences_.signal();
        publishing_ = false;
        note_work();
        return false;
    }
    const bool publishing = polled == HostPoll::pending;
    if (settle()) fences_.signal();
    publishing_ = publishing;
    note_work();
    return publishing;
}

void RingConsumer::announce_awake(bool awake)
{
    AV_SCOPE("RingConsumer::announce_awake");
    if (attached_) static_cast<void>(ring_.announce_awake(awake));
    if (!awake && publishing_) {
        publishing_ = false;
        note_work();
    }
}

void RingConsumer::note_work()
{
    AV_SCOPE("RingConsumer::note_work");
    fences_.at_work(serving_ || publishing_);
}

uint64_t RingConsumer::earliest_pending() const
{
    AV_SCOPE("RingConsumer::earliest_pending");
    if (!attached_ || faulted_ || !executor_.ready()) return UINT64_MAX;
    const uint64_t running_end = __atomic_load_n(&running_end_, __ATOMIC_ACQUIRE);
    const uint64_t head = running_end != 0u ? running_end : __atomic_load_n(&head_, __ATOMIC_ACQUIRE);
    RingHeader     header;
    if (ring_.snapshot(header) == Status::ok && header.tail() - head >= BatchHeader::encoded_size) {
        uint8_t     bytes[BatchHeader::encoded_size] = {};
        BatchHeader batch;
        if (ring_.copy(head, bytes, sizeof bytes) != Status::ok || batch.decode(bytes, sizeof bytes) != Status::ok)
            return 0u;
        return batch.published_at();
    }
    return UINT64_MAX;
}

void RingConsumer::note_clock(uint64_t published_at)
{
    AV_SCOPE("RingConsumer::note_clock");
    constexpr uint64_t every = 16u;
    if (!clock_pairs_ || published_at == 0u || batches_ % every != 0u) return;
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    const uint64_t host = (static_cast<uint64_t>(now.tv_sec) * 1000000000u + static_cast<uint64_t>(now.tv_nsec)) / 100u;
    fprintf(stderr, "av-clock host=%llu guest=%llu\n", static_cast<unsigned long long>(host),
            static_cast<unsigned long long>(published_at));
}

void RingConsumer::order_with(D3dOrdering &ordering)
{
    AV_SCOPE("RingConsumer::order_with");
    ordering_ = &ordering;
}

void RingConsumer::fault(FaultCode code, const char *reason, uint64_t value)
{
    AV_SCOPE("RingConsumer::fault");
    char       bytes[160];
    TextBuffer message(bytes, sizeof bytes);
    message.append("virtio-gpu-av: context faulted, code ");
    message.append_decimal(static_cast<uint32_t>(code));
    message.append(": ");
    message.append(reason);
    message.append(" ");
    message.append_decimal(value);
    host_.log(message.text());
    faulted_ = true;
    executor_.stop();
    fences_.fault(code);
}

bool     RingConsumer::attached() const
{
    AV_SCOPE("RingConsumer::attached");
    return attached_;
}
bool     RingConsumer::faulted() const
{
    AV_SCOPE("RingConsumer::faulted");
    return faulted_;
}
uint64_t RingConsumer::head() const
{
    AV_SCOPE("RingConsumer::head");
    return head_;
}
uint64_t RingConsumer::last_fence() const
{
    AV_SCOPE("RingConsumer::last_fence");
    return last_fence_;
}
uint64_t RingConsumer::batches() const
{
    AV_SCOPE("RingConsumer::batches");
    return batches_;
}

bool RingConsumer::invariants_hold() const
{
    AV_SCOPE("RingConsumer::invariants_hold");
    if (!prepared_) return !attached_ && head_ == 0u;
    return progress_.invariants_hold() && head_ <= progress_.tail() &&
           batches_ <= head_ / BatchHeader::encoded_size && batches_ <= last_fence_;
}

}
