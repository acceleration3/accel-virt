#include "device/threading/worker.hpp"

#include "device/abi/entry_points.hpp"
#include "device/host/host.hpp"
#include "device/ring/ring_consumer.hpp"

namespace av {

Worker::Worker(Host &host, RingConsumer &ring)
    : host_(host), ring_(ring), doorbell_(host), thread_(nullptr), wake_(nullptr), lock_(nullptr), queue_{}, head_(0u), count_(0u),
      stopping_(false) {}

Worker::~Worker() { stop(); }

Status Worker::start()
{
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
    host_.require_main_loop("Worker::connect_doorbell");
    if (thread_ == nullptr) return Status::invalid_state;
    return doorbell_.connect(wake_, index);
}

void Worker::disconnect_doorbell()
{
    host_.require_main_loop("Worker::disconnect_doorbell");
    doorbell_.disconnect();
}

void Worker::stop()
{
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
    return thread_ != nullptr;
}

uint32_t Worker::pending()
{
    host_.require_main_loop("Worker::pending");
    if (thread_ == nullptr) return 0u;
    host_.lock_mutex(lock_);
    const uint32_t queued = count_;
    host_.unlock_mutex(lock_);
    return queued;
}

bool Worker::stopping()
{
    if (thread_ == nullptr) return false;
    host_.lock_mutex(lock_);
    const bool requested = stopping_;
    host_.unlock_mutex(lock_);
    return requested;
}

Worker::Step Worker::next(WorkItem &item)
{
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
    host_.require_worker("Worker::run");
    for (;;) {
        host_.wait_event(wake_);
        WorkItem item;
        for (Step step = next(item); step != Step::idle; step = next(item)) {
            if (step == Step::stop) return;
            item.run();
            item.complete_on(host_);
        }
        ring_.service();
    }
}

/* A doorbell rings only a running worker's event. */
bool Worker::invariants_hold() const
{
    return count_ <= queue_capacity && head_ < queue_capacity && doorbell_.invariants_hold() &&
           (thread_ != nullptr || !doorbell_.connected());
}

}  // namespace av
