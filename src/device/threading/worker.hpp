#ifndef AV_WORKER_HPP
#define AV_WORKER_HPP

#include "core/support/status.hpp"
#include "device/threading/doorbell.hpp"
#include "device/threading/work_item.hpp"

#include <stdint.h>

namespace av {

class ContextSignal;
class Host;
class StateWriter;
class RingConsumer;
struct WorkerProbe;

class Worker {
public:
    static constexpr uint32_t queue_capacity = 64u;

    Worker(Host &host, RingConsumer &ring);
    ~Worker();
    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;

    [[nodiscard]] Status start();
    [[nodiscard]] Status post(const WorkItem &item);
    [[nodiscard]] Status connect_doorbell(uint32_t index);
    void                 connect_signal(ContextSignal &signal);
    void                 wake();
    void                 disconnect_doorbell();
    void                 stop();
    bool                 running() const;
    uint32_t             pending();
    bool                 stopping();
    void                 run();
    void                 report(StateWriter &writer);

    bool invariants_hold() const;

private:
    enum class Step { work, idle, stop };

    Step next(WorkItem &item);
    bool work_within_spin();
    void release();

    Host          &host_;
    RingConsumer  &ring_;
    ContextSignal *signal_;
    Doorbell       doorbell_;
    void          *thread_;
    void          *wake_;
    void          *lock_;
    WorkItem       queue_[queue_capacity];
    uint32_t       head_;
    uint32_t       count_;
    bool           stopping_;

    friend struct WorkerProbe;
};

}

#endif
