#include "device/ring/ring_consumer.hpp"

#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "device/support/text_buffer.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

RingConsumer::RingConsumer(Host &host, FenceReporter &fences)
    : host_(host), fences_(fences), ring_(), progress_(), head_(0u), last_fence_(0u), batches_(0u), prepared_(false),
      attached_(false), stopped_(false) {}

Status RingConsumer::prepare(const RingWindow &ring)
{
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
    host_.require_main_loop("RingConsumer::withdraw");
    ring_ = RingWindow();
    progress_ = RingProgress();
    prepared_ = false;
}

void RingConsumer::attach()
{
    host_.require_worker("RingConsumer::attach");
    attached_ = prepared_;
}

/* The header's head field is the guest's to scribble on: only the device's own
 * head is judged and published. */
void RingConsumer::service()
{
    host_.require_worker("RingConsumer::service");
    if (!attached_ || stopped_) return;
    RingHeader snapshot;
    if (ring_.snapshot(snapshot) != Status::ok) return stop("ring header unreadable or malformed, order", ring_.order());
    if (snapshot.order() != ring_.order()) return stop("ring order changed to", snapshot.order());
    if (progress_.observe(RingHeader(snapshot.order(), snapshot.tail(), head_)) != Status::ok)
        return stop("ring tail inconsistent with what was consumed:", snapshot.tail());
    while (consume_next() == Status::ok) {}
    fences_.signal();
}

Status RingConsumer::consume_next()
{
    const uint64_t published = progress_.tail() - head_;
    if (published < BatchHeader::encoded_size) return Status::no_space;
    uint8_t     bytes[BatchHeader::encoded_size] = {};
    BatchHeader batch;
    if (ring_.copy(head_, bytes, sizeof bytes) != Status::ok || batch.decode(bytes, sizeof bytes) != Status::ok) {
        stop("batch header unreadable or malformed at head", head_);
        return Status::malformed;
    }
    if (!batch.fits(ring_.capacity())) {
        stop("batch cannot fit the ring, payload bytes", batch.length());
        return Status::malformed;
    }
    if (batch.fence_id() <= last_fence_) {
        stop("batch fence does not follow the last, fence", batch.fence_id());
        return Status::malformed;
    }
    if (published < batch.span()) return Status::no_space;
    head_ += batch.span();
    last_fence_ = batch.fence_id();
    batches_++;
    /* The header was just read, so its head field is inside the blob. */
    static_cast<void>(ring_.publish_head(head_));
    fences_.complete(last_fence_);
    return Status::ok;
}

void RingConsumer::stop(const char *reason, uint64_t value)
{
    char       bytes[160];
    TextBuffer message(bytes, sizeof bytes);
    message.append("virtio-gpu-rvk: context ring stopped: ");
    message.append(reason);
    message.append(" ");
    message.append_decimal(value);
    host_.log(message.text());
    stopped_ = true;
}

bool     RingConsumer::attached() const   { return attached_; }
bool     RingConsumer::stopped() const    { return stopped_; }
uint64_t RingConsumer::head() const       { return head_; }
uint64_t RingConsumer::last_fence() const { return last_fence_; }
uint64_t RingConsumer::batches() const    { return batches_; }

/* Every batch spans at least a header and carries a larger fence than the one
 * before it, starting from 1; nothing is consumed that was not published. */
bool RingConsumer::invariants_hold() const
{
    if (!prepared_) return !attached_ && head_ == 0u;
    return progress_.invariants_hold() && head_ <= progress_.tail() &&
           batches_ <= head_ / BatchHeader::encoded_size && batches_ <= last_fence_;
}

}  // namespace av
