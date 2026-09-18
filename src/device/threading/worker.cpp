#include "device/threading/worker.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/entry_points.hpp"
#include "device/context/context_signal.hpp"
#include "device/host/host.hpp"
#include "device/ring/ring_consumer.hpp"

namespace av {

Worker::Worker(Host &host, RingConsumer &ring)
    : host_(host), ring_(ring), signal_(nullptr), doorbell_(host), thread_(nullptr), wake_(nullptr), lock_(nullptr), queue_{}, head_(0u), count_(0u),
      stopping_(false)
      {
          AV_SCOPE("Worker::Worker");
      }

Worker::~Worker()
{
    AV_SCOPE("Worker::~Worker");
    stop();
}

Status Worker::start()
{
    AV_SCOPE("Worker::start");
    host_.require_main_loop("Worker::start");
    if (thread_ != nullptr) return Status::invalid_state;
    stopping_ = false;
    Status status = host_.create_event(wake_);
    if (status == Status::ok) status = host_.create_mutex(lock_);
    if (status == Status::ok) status = host_.start_thread(&RvkWorkerRun, this, thread_);
    if (status != Status::ok) release();
    return status;
}

Status Worker::post(const WorkItem &item)
{
    AV_SCOPE("Worker::post");
    host_.require_main_loop("Worker::post");
    if (thread_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    const bool room = count_ < queue_capacity;
    if (room) queue_[(head_ + count_++) % queue_capacity] = item;
    host_.unlock_mutex(lock_);
    if (!room) return Status::no_space;
    host_.signal_event(wake_);
    return Status::ok;
}

Status Worker::connect_doorbell(uint32_t index)
{
    AV_SCOPE("Worker::connect_doorbell");
    host_.require_main_loop("Worker::connect_doorbell");
    if (thread_ == nullptr) return Status::invalid_state;
    return doorbell_.connect(wake_, index);
}

void Worker::connect_signal(ContextSignal &signal)
{
    AV_SCOPE("Worker::connect_signal");
    host_.require_main_loop("Worker::connect_signal");
    signal_ = &signal;
}

/* The event outlives every wake: the landing pad is closed on the main loop,
 * before stop() destroys what it signals. */
void Worker::wake()
{
    AV_SCOPE("Worker::wake");
    if (wake_ == nullptr) return;
    host_.signal_event(wake_);
}

void Worker::disconnect_doorbell()
{
    AV_SCOPE("Worker::disconnect_doorbell");
    host_.require_main_loop("Worker::disconnect_doorbell");
    doorbell_.disconnect();
}

void Worker::stop()
{
    AV_SCOPE("Worker::stop");
    host_.require_main_loop("Worker::stop");
    if (thread_ == nullptr) return;
    doorbell_.disconnect();
    host_.lock_mutex(lock_);
    stopping_ = true;
    host_.unlock_mutex(lock_);
    host_.signal_event(wake_);
    host_.join_thread(thread_);
    release();
}

void Worker::release()
{
    AV_SCOPE("Worker::release");
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
    if (wake_ != nullptr) host_.destroy_event(wake_);
    thread_ = nullptr;
    lock_ = nullptr;
    wake_ = nullptr;
    head_ = 0u;
    count_ = 0u;
}

bool Worker::running() const
{
    AV_SCOPE("Worker::running");
    return thread_ != nullptr;
}

uint32_t Worker::pending()
{
    AV_SCOPE("Worker::pending");
    host_.require_main_loop("Worker::pending");
    if (thread_ == nullptr) return 0u;
    host_.lock_mutex(lock_);
    const uint32_t queued = count_;
    host_.unlock_mutex(lock_);
    return queued;
}

void Worker::report(StateWriter &writer)
{
    AV_SCOPE("Worker::report");
    writer.open("worker", 0u);
    writer.flag("running", thread_ != nullptr);
    uint32_t queued = 0u;
    bool     requested = false;
    if (thread_ != nullptr) {
        if (!host_.try_lock_mutex(lock_)) {
            writer.flag("busy", true);
            writer.close();
            return;
        }
        queued = count_;
        requested = stopping_;
        host_.unlock_mutex(lock_);
    }
    writer.number("pending", queued);
    writer.flag("stopping", requested);
    writer.close();
}

bool Worker::stopping()
{
    AV_SCOPE("Worker::stopping");
    if (thread_ == nullptr) return false;
    host_.lock_mutex(lock_);
    const bool requested = stopping_;
    host_.unlock_mutex(lock_);
    return requested;
}

Worker::Step Worker::next(WorkItem &item)
{
    AV_SCOPE("Worker::next");
    host_.lock_mutex(lock_);
    Step step = stopping_ ? Step::stop : count_ == 0u ? Step::idle : Step::work;
    if (step == Step::work) {
        item = queue_[head_];
        head_ = (head_ + 1u) % queue_capacity;
        count_--;
    }
    host_.unlock_mutex(lock_);
    return step;
}

void Worker::run()
{
    AV_SCOPE("Worker::run");
    host_.require_worker("Worker::run");
    for (;;) {
        host_.wait_event(wake_);
        WorkItem item;
        for (Step step = next(item); step != Step::idle; step = next(item)) {
            if (step == Step::stop) return ring_.detach();
            item.run();
            item.complete_on(host_);
        }
        if (signal_ != nullptr) signal_->drain();
        ring_.service();
    }
}

/* A doorbell rings only a running worker's event. */
bool Worker::invariants_hold() const
{
    AV_SCOPE("Worker::invariants_hold");
    return count_ <= queue_capacity && head_ < queue_capacity && doorbell_.invariants_hold() &&
           (thread_ != nullptr || !doorbell_.connected());
}

}  // namespace av
