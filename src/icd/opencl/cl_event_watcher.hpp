#ifndef AV_CLICD_CL_EVENT_WATCHER_HPP
#define AV_CLICD_CL_EVENT_WATCHER_HPP

#include "core/support/growable_array.hpp"
#include "icd/common/service_thread.hpp"
#include "icd/opencl/cl_event_owner.hpp"
#include "icd/opencl/cl_watched_event.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
class ThreadLock;
}

namespace av::clicd {

class ClObject;
struct ClEventWatcherProbe;

class ClEventWatcher final : public icd::ServiceLoop {
public:
    static constexpr uint32_t asks_per_turn = 8u;

    ClEventWatcher(BlockSource &memory, icd::ServiceThread &thread, DriverLog &log, ThreadLock &lock,
                   ClEventOwner &owner);
    ~ClEventWatcher();
    ClEventWatcher(const ClEventWatcher &) = delete;
    ClEventWatcher &operator=(const ClEventWatcher &) = delete;

    cl_int watch(ClObject *event, uint64_t wire, ClientTransport *wire_of_context,
                 cl_int status_wanted, ClWatchedEvent::Notify notify, void *user_data, bool driven);

    uint64_t wake_handle() const;
    bool     attend();
    uint64_t woken() const;
    void     await_woken(uint64_t seen, uint64_t timeout);

    bool turn() override;

    void forget_context(const ClientTransport *wire_of_context);

    uint32_t registered() const;
    uint64_t delivered() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct ClEventWatcherProbe;


    bool completed(const ClWatchedEvent &registration, cl_int &status) const;
    bool deliver(uint32_t slot, const ClWatchedEvent &asked, cl_int status);
    bool mark_oldest();

    icd::ServiceThread &thread_;
    DriverLog     &log_;
    ThreadLock         &lock_;
    ClEventOwner       &owner_;
    struct Row {
        ClWatchedEvent registration_;
        bool           driven_;
        bool           oldest_;
    };

    struct QueueOldest {
        const ClObject *queue_;
        uint32_t        row_;
    };

    GrowableArray<Row>         rows_;
    GrowableArray<QueueOldest> queues_;
    uint32_t            held_;
    uint64_t            delivered_;
    uint32_t            next_;
};

}

#endif
