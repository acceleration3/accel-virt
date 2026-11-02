#include "device/present/present_engine.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/entry_points.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/host/host.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "proto/pages/present_progress.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

namespace {

/* What the log says of a present the CPU moved, by QueuedPresent::Fallback. */
constexpr const char *fallback_texts[] = {
    nullptr,
    "virtio-gpu-av: presents are copied through the CPU: the guest swaps their red and blue",
    "virtio-gpu-av: presents are copied through the CPU: their destination pages are not lent to the GPU",
    "virtio-gpu-av: presents are copied through the CPU: their destination pages are not one piece of a lending",
    "virtio-gpu-av: presents are copied through the CPU: no descriptor for the lending's memory",
    "virtio-gpu-av: presents are copied through the CPU: the lending they were taken for was given back",
    "virtio-gpu-av: presents are copied through the CPU: their regions have no layout in the lending's memory",
    "virtio-gpu-av: presents are copied through the CPU: the executor cannot copy their frames to the lending "
    "(import, format or API)",
};
static_assert(sizeof fallback_texts / sizeof fallback_texts[0] == static_cast<uint32_t>(QueuedPresent::Fallback::count),
              "every fallback has its words");

}  // namespace

PresentEngine::PresentEngine(Host &host, FenceInterrupts &interrupts)
    : host_(host), interrupts_(interrupts), lock_(nullptr), wake_(nullptr), idle_(nullptr), thread_(nullptr), stopping_(false), holding_(false), moving_(false),
      page_(host), snapshots_(), lendings_(nullptr), told_{}, slots_(), first_(0u), moved_(0u), held_(0u), counted_(0u), completed_(0u), refused_(0u)
{
    AV_SCOPE("PresentEngine::PresentEngine");
    for (ObjectStorage<QueuedPresent> &storage : slots_) static_cast<void>(storage.construct(host));
}

PresentEngine::~PresentEngine()
{
    AV_SCOPE("PresentEngine::~PresentEngine");
    stop_thread();
    unregister();
    for (ObjectStorage<QueuedPresent> &storage : slots_) storage.destroy();
    if (idle_ != nullptr) host_.destroy_event(idle_);
    if (wake_ != nullptr) host_.destroy_event(wake_);
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status PresentEngine::initialize()
{
    AV_SCOPE("PresentEngine::initialize");
    host_.require_main_loop("PresentEngine::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    Status status = host_.create_mutex(lock_);
    if (status == Status::ok) status = host_.create_event(wake_);
    if (status == Status::ok) status = host_.create_event(idle_);
    if (status == Status::ok) return Status::ok;
    if (wake_ != nullptr) host_.destroy_event(wake_);
    wake_ = nullptr;
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
    lock_ = nullptr;
    return status;
}

QueuedPresent &PresentEngine::slot(uint32_t index)
{
    AV_SCOPE("PresentEngine::slot");
    return slots_[index % depth].object();
}

Status PresentEngine::attach(uint64_t address, const PresentFrames &snapshots, GuestRamLendings &lendings)
{
    AV_SCOPE("PresentEngine::attach");
    host_.require_main_loop("PresentEngine::attach");
    if (lock_ == nullptr || wake_ == nullptr || idle_ == nullptr) return Status::invalid_state;
    detach();
    if (address == 0u) return Status::ok;
    snapshots_ = snapshots;
    const Status mapped = page_.map(address);
    if (mapped != Status::ok) return mapped;
    /* Known to the swaps of guest RAM before it can move a present. */
    const Status known = lendings.swaps().add(*this);
    if (known != Status::ok) {
        page_.release();
        return known;
    }
    lendings_ = &lendings;
    stopping_ = false;
    holding_ = false;
    moving_ = false;
    if (host_.start_thread(&AvPresentEngineRun, this, thread_) == Status::ok) return Status::ok;
    unregister();
    page_.release();
    return Status::no_memory;
}

void PresentEngine::unregister()
{
    AV_SCOPE("PresentEngine::unregister");
    if (lendings_ != nullptr) lendings_->swaps().remove(*this);
    lendings_ = nullptr;
}

void PresentEngine::stop_thread()
{
    AV_SCOPE("PresentEngine::stop_thread");
    if (thread_ == nullptr) return;
    host_.lock_mutex(lock_);
    stopping_ = true;
    host_.unlock_mutex(lock_);
    host_.signal_event(wake_);
    host_.join_thread(thread_);
    thread_ = nullptr;
}

void PresentEngine::detach()
{
    AV_SCOPE("PresentEngine::detach");
    host_.require_main_loop("PresentEngine::detach");
    if (lock_ == nullptr) return;
    stop_thread();
    for (uint32_t index = 0u; index < depth; index++) slot(index).release();
    first_ = 0u;
    moved_ = 0u;
    held_ = 0u;
    counted_ = 0u;
    completed_ = 0u;
    refused_ = 0u;
    for (bool &told : told_) told = false;
    unregister();
    page_.release();
}

uint32_t PresentEngine::queue(ControlRequest &request)
{
    AV_SCOPE("PresentEngine::queue");
    host_.require_main_loop("PresentEngine::queue");
    if (lock_ == nullptr || !page_.mapped()) return GpuResponse::err_invalid_parameter;
    host_.lock_mutex(lock_);
    release_moved();
    const uint64_t ticket = ++counted_;
    host_.unlock_mutex(lock_);
    return take(request, ticket);
}

void PresentEngine::release_moved()
{
    AV_SCOPE("PresentEngine::release_moved");
    for (; moved_ != 0u; moved_--, held_--, first_ = (first_ + 1u) % depth) slot(first_).release();
}

/* Only the main loop changes first_ and held_, so the slot past the held ones
 * is its own to fill without the lock; the thread learns of it once held_
 * counts it. */
uint32_t PresentEngine::take(ControlRequest &request, uint64_t ticket)
{
    AV_SCOPE("PresentEngine::take");
    uint8_t              fixed[QueuedPresentRequest::encoded_size] = {};
    WireReader           reader(fixed, request.read(0u, fixed, sizeof fixed));
    QueuedPresentRequest present;
    Status               status = present.decode(reader);
    if (status == Status::ok && (present.ticket() != ticket || held_ == depth)) status = Status::invalid_parameter;
    if (status == Status::ok) status = slot(first_ + held_).accept(request, present, snapshots_, *lendings_);
    host_.lock_mutex(lock_);
    const bool completed_now = status != Status::ok && refuse();
    if (status == Status::ok) held_++;
    host_.unlock_mutex(lock_);
    if (status == Status::ok) host_.signal_event(wake_);
    if (completed_now) interrupts_.raise(FenceInterrupts::present_line);
    if (status == Status::ok) return GpuResponse::ok_nodata;
    return status == Status::no_memory ? GpuResponse::err_out_of_memory : GpuResponse::err_invalid_parameter;
}

bool PresentEngine::refuse()
{
    AV_SCOPE("PresentEngine::refuse");
    if (moved_ != held_) {
        slot(first_ + held_ - 1u).follow_with_refusal();
        return false;
    }
    completed_ = counted_;
    refused_++;
    page_.publish(PresentProgress(completed_, refused_));
    return true;
}

void PresentEngine::run()
{
    AV_SCOPE("PresentEngine::run");
    for (QueuedPresent *present = next(); present != nullptr; present = next()) {
        const Status moved = present->move(snapshots_);
        note_fallback(present->fallback());
        complete(*present, moved);
    }
}

void PresentEngine::note_fallback(QueuedPresent::Fallback fallback)
{
    AV_SCOPE("PresentEngine::note_fallback");
    const uint32_t index = static_cast<uint32_t>(fallback);
    if (fallback == QueuedPresent::Fallback::none || told_[index]) return;
    told_[index] = true;
    host_.log(fallback_texts[index]);
}

QueuedPresent *PresentEngine::next()
{
    AV_SCOPE("PresentEngine::next");
    host_.lock_mutex(lock_);
    while (!stopping_ && (moved_ == held_ || holding_)) {
        host_.unlock_mutex(lock_);
        host_.wait_event(wake_);
        host_.lock_mutex(lock_);
    }
    QueuedPresent *const present = stopping_ ? nullptr : &slot(first_ + moved_);
    moving_ = present != nullptr;
    host_.unlock_mutex(lock_);
    return present;
}

void PresentEngine::complete(QueuedPresent &present, Status moved)
{
    AV_SCOPE("PresentEngine::complete");
    host_.lock_mutex(lock_);
    moved_++;
    moving_ = false;
    completed_ = present.ticket() + present.refusals_after();
    refused_ += present.refusals_after() + (moved == Status::ok ? 0u : 1u);
    page_.publish(PresentProgress(completed_, refused_));
    host_.unlock_mutex(lock_);
    host_.signal_event(idle_);
    interrupts_.raise(FenceInterrupts::present_line);
}

void PresentEngine::hold_moves()
{
    AV_SCOPE("PresentEngine::hold_moves");
    host_.require_main_loop("PresentEngine::hold_moves");
    if (lock_ == nullptr || thread_ == nullptr) return;
    host_.lock_mutex(lock_);
    holding_ = true;
    while (moving_) {
        host_.unlock_mutex(lock_);
        host_.wait_event(idle_);
        host_.lock_mutex(lock_);
    }
    host_.unlock_mutex(lock_);
}

void PresentEngine::resume_moves()
{
    AV_SCOPE("PresentEngine::resume_moves");
    host_.require_main_loop("PresentEngine::resume_moves");
    if (lock_ == nullptr || thread_ == nullptr) return;
    host_.lock_mutex(lock_);
    holding_ = false;
    host_.unlock_mutex(lock_);
    host_.signal_event(wake_);
}

bool PresentEngine::invariants_hold() const
{
    AV_SCOPE("PresentEngine::invariants_hold");
    if (first_ >= depth || moved_ > held_ || held_ > depth) return false;
    if (refused_ > completed_ || completed_ > counted_) return false;
    for (const ObjectStorage<QueuedPresent> &storage : slots_)
        if (!storage.constructed() || !storage.object().invariants_hold()) return false;
    return thread_ == nullptr || (lock_ != nullptr && wake_ != nullptr && page_.mapped());
}

}  // namespace av
