#include "guest/transport/guest_doorbell.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

namespace {

/* What a store to this device's doorbell is: one four-byte write at offset 0
 * of the page (rvk_device.h). The value carries nothing - the ring's tail is
 * what says there is work - so it is the same every time. */
constexpr uint32_t store_value = 1u;

}  // namespace

GuestDoorbell::GuestDoorbell(Kmt &kmt)
    : kmt_(&kmt), device_(0u), context_(0u), ring_(0u), queue_(0u), doorbell_(0u), progress_(nullptr),
      page_(nullptr), status_(nullptr), queued_(nullptr)
{
    AV_SCOPE("GuestDoorbell::GuestDoorbell");
}

GuestDoorbell::~GuestDoorbell()
{
    AV_SCOPE("GuestDoorbell::~GuestDoorbell");
    teardown();
}

Status GuestDoorbell::initialize(uint32_t device, uint32_t context, uint32_t ring)
{
    AV_SCOPE("GuestDoorbell::initialize");
    if (established()) return Status::invalid_state;
    uint32_t        queue = 0u;
    const uint64_t *progress = nullptr;
    const Status    made = kmt_->create_hw_queue(device, context, progress, queue);
    if (made != Status::ok) return made;
    uint32_t                 doorbell = 0u;
    void                    *page = nullptr;
    const uint32_t          *status = nullptr;
    uint64_t                *queued = nullptr;
    const Status             opened = kmt_->create_doorbell(queue, ring, doorbell, page, status, queued);
    if (opened != Status::ok || page == nullptr || status == nullptr || queued == nullptr) {
        /* Nothing half-made: a queue with no doorbell on it is a queue this
         * process has no use for. */
        static_cast<void>(kmt_->destroy_hw_queue(queue));
        return opened != Status::ok ? opened : Status::invalid_state;
    }
    device_ = device;
    context_ = context;
    ring_ = ring;
    queue_ = queue;
    doorbell_ = doorbell;
    progress_ = progress;
    page_ = static_cast<volatile uint32_t *>(page);
    status_ = status;
    queued_ = queued;
    return Status::ok;
}

Status GuestDoorbell::connect()
{
    AV_SCOPE("GuestDoorbell::connect");
    if (!established()) return Status::invalid_state;
    return kmt_->connect_doorbell(doorbell_);
}

void GuestDoorbell::teardown()
{
    AV_SCOPE("GuestDoorbell::teardown");
    if (doorbell_ != 0u) static_cast<void>(kmt_->destroy_doorbell(doorbell_));
    if (queue_ != 0u) static_cast<void>(kmt_->destroy_hw_queue(queue_));
    queue_ = 0u;
    doorbell_ = 0u;
    progress_ = nullptr;
    /* device_, context_ and ring_ stay: they are what another doorbell of this
     * context would be made from. */
    page_ = nullptr;
    status_ = nullptr;
    queued_ = nullptr;
}

bool GuestDoorbell::established() const
{
    AV_SCOPE("GuestDoorbell::established");
    return doorbell_ != 0u;
}

DoorbellStatus GuestDoorbell::status() const
{
    AV_SCOPE("GuestDoorbell::status");
    if (status_ == nullptr) return DoorbellStatus::disconnected_abort;
    /* The page is written by dxgkrnl, so a value outside the enumeration is a
     * value to refuse rather than to load as one. */
    const uint32_t read = *status_;
    if (read == static_cast<uint32_t>(DoorbellStatus::connected)) return DoorbellStatus::connected;
    if (read == static_cast<uint32_t>(DoorbellStatus::connected_notify_kmd))
        return DoorbellStatus::connected_notify_kmd;
    if (read == static_cast<uint32_t>(DoorbellStatus::disconnected_retry))
        return DoorbellStatus::disconnected_retry;
    return DoorbellStatus::disconnected_abort;
}

bool GuestDoorbell::ring(uint64_t fence)
{
    AV_SCOPE("GuestDoorbell::ring");
    const DoorbellStatus now = status();
    if (now != DoorbellStatus::connected && now != DoorbellStatus::connected_notify_kmd) return false;
    *queued_ = fence;
    *page_ = store_value;
    /* Only where the driver asked to hear about it, which this device's never
     * does (research.md §7). */
    if (now == DoorbellStatus::connected_notify_kmd)
        static_cast<void>(kmt_->notify_work_submission(doorbell_));
    return true;
}

bool GuestDoorbell::recover()
{
    AV_SCOPE("GuestDoorbell::recover");
    const DoorbellStatus now = status();
    if (now == DoorbellStatus::connected || now == DoorbellStatus::connected_notify_kmd) return true;
    /* A retry is the same doorbell asked for again; a status page can only say
     * retry while there is one to ask about. */
    if (now == DoorbellStatus::disconnected_retry) return kmt_->connect_doorbell(doorbell_) == Status::ok;
    /* Abort: this doorbell is gone for good, and what a process does about it
     * is make another on the same context. */
    teardown();
    if (ring_ == 0u) return false;
    if (initialize(device_, context_, ring_) != Status::ok) return false;
    return connect() == Status::ok;
}

uint32_t GuestDoorbell::handle() const
{
    AV_SCOPE("GuestDoorbell::handle");
    return doorbell_;
}

bool GuestDoorbell::invariants_hold() const
{
    AV_SCOPE("GuestDoorbell::invariants_hold");
    const bool mapped = page_ != nullptr && status_ != nullptr && queued_ != nullptr;
    if (mapped != established()) return false;
    return !established() || queue_ != 0u;
}

}  // namespace av
