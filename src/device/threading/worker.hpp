/* The thread one context's work runs on, so no decode or execution happens on
 * QEMU's main loop. The main loop posts work items; the worker runs them in
 * posting order and hands each completion back to the main loop. Each time it
 * wakes - for posted work or a store to its doorbell - it drains the queue,
 * then serves its context's ring, so a doorbell that raced the ring's
 * attachment is served by it. Stopping disconnects the doorbell, drops work
 * not yet started, finishes the item running, detaches the ring on the
 * worker's own thread, and joins. */
#ifndef AV_WORKER_HPP
#define AV_WORKER_HPP

#include "core/support/status.hpp"
#include "device/threading/doorbell.hpp"
#include "device/threading/work_item.hpp"

#include <stdint.h>

namespace av {

class Host;
class RingConsumer;
struct WorkerProbe;

class Worker {
public:
    static constexpr uint32_t queue_capacity = 64u;

    Worker(Host &host, RingConsumer &ring);
    ~Worker();
    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;

    /* Main loop. no_memory, having released what it made, when the host cannot
     * create the thread or what it waits on. */
    [[nodiscard]] Status start();
    /* Main loop. invalid_state unless started; no_space when the queue is full. */
    [[nodiscard]] Status post(const WorkItem &item);
    /* Main loop. invalid_state unless started or once connected; the
     * doorbell's answer otherwise. */
    [[nodiscard]] Status connect_doorbell(uint32_t index);
    /* Main loop. */
    void                 disconnect_doorbell();
    void                 stop();
    bool                 running() const;
    uint32_t             pending();
    bool                 stopping();
    /* The thread's body, entered through RvkWorkerRun. */
    void                 run();

    bool invariants_hold() const;

private:
    enum class Step { work, idle, stop };

    Step next(WorkItem &item);
    void release();

    Host         &host_;
    RingConsumer &ring_;
    /* Rings wake_: stop() disconnects it before the event is destroyed. */
    Doorbell      doorbell_;
    void         *thread_;
    void         *wake_;
    void         *lock_;
    WorkItem      queue_[queue_capacity];
    uint32_t      head_;
    uint32_t      count_;
    bool          stopping_;

    friend struct WorkerProbe;
};

}  // namespace av

#endif  // AV_WORKER_HPP
