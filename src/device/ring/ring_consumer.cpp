#include "device/ring/ring_consumer.hpp"

#include "device/context/d3d_ordering.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/batch_executor.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "core/support/text_buffer.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

RingConsumer::RingConsumer(Host &host, FenceReporter &fences, BatchExecutor &batches)
    : host_(host), fences_(fences), executor_(batches), ring_(), progress_(), head_(0u), last_fence_(0u), batches_(0u), ordering_(nullptr), prepared_(false),
      attached_(false), faulted_(false)
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

void RingConsumer::detach()
{
    AV_SCOPE("RingConsumer::detach");
    host_.require_worker("RingConsumer::detach");
    executor_.stop();
}

/* The header's head field is the guest's to scribble on: only the device's own
 * head is judged and published. */
void RingConsumer::service()
{
    AV_SCOPE("RingConsumer::service");
    host_.require_worker("RingConsumer::service");
    if (!attached_ || faulted_) return;
    serve();
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
    while (consume_next() == Status::ok) {}
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
    if (ordering_ != nullptr) ordering_->wait_before(*this, batch.published_at());
    const Status ran = executor_.run(ring_, head_, batch);
    if (ran != Status::ok) {
        fault(executor_.fault(), "the batch could not run, fence", batch.fence_id());
        return ran;
    }
    __atomic_store_n(&head_, head_ + batch.span(), __ATOMIC_RELEASE);
    last_fence_ = batch.fence_id();
    batches_++;
    /* The header was just read, so its head field is inside the blob. */
    static_cast<void>(ring_.publish_head(head_));
    fences_.complete(last_fence_);
    return Status::ok;
}

/* The executor goes at once: the host API's objects are the device's to free,
 * the context's binding the guest's. */
uint64_t RingConsumer::earliest_pending() const
{
    AV_SCOPE("RingConsumer::earliest_pending");
    if (!attached_ || faulted_ || !executor_.ready()) return UINT64_MAX;
    const uint64_t head = __atomic_load_n(&head_, __ATOMIC_ACQUIRE);
    RingHeader     header;
    if (ring_.snapshot(header) == Status::ok && header.tail() - head >= BatchHeader::encoded_size) {
        uint8_t     bytes[BatchHeader::encoded_size] = {};
        BatchHeader batch;
        /* A header torn by the owner moving on is read as due now. */
        if (ring_.copy(head, bytes, sizeof bytes) != Status::ok || batch.decode(bytes, sizeof bytes) != Status::ok)
            return 0u;
        return batch.published_at();
    }
    /* A batch the guest announced but its kernel has not handed over is not
     * waited for: the kernel may be holding it until the waiting stream's own
     * packet completes, and the two would wait on each other. */
    return UINT64_MAX;
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
    message.append("virtio-gpu-rvk: context faulted, code ");
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

/* Every batch spans at least a header and carries a larger fence than the one
 * before it, starting from 1; nothing is consumed that was not published. */
bool RingConsumer::invariants_hold() const
{
    AV_SCOPE("RingConsumer::invariants_hold");
    if (!prepared_) return !attached_ && head_ == 0u;
    return progress_.invariants_hold() && head_ <= progress_.tail() &&
           batches_ <= head_ / BatchHeader::encoded_size && batches_ <= last_fence_;
}

}  // namespace av
