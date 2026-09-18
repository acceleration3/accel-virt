/* The adapter's doorbells (feature 049, US1): the ones processes asked for on
 * hardware queues, by handle, each naming the device page its queue's context
 * was bound to.
 *
 * One per adapter, taken at PASSIVE only - every doorbell DDI is PASSIVE
 * (d3dkmddi.h:10288) - so a mutex, as the hardware queues' table has. The
 * address it answers is a page of the doorbell BAR the adapter mapped, which
 * dxgkrnl maps into the process that asked. */
#ifndef AV_DOORBELL_TABLE_HPP
#define AV_DOORBELL_TABLE_HPP

#include "core/ddi/dxgkarg_doorbell_records.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/scheduling/queue_doorbell.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class HwQueue;
class MappedRegisterWindow;
class StateWriter;
struct DoorbellTableProbe;

class DoorbellTable {
public:
    /* One per doorbell page and one spare per queue: a process may hold a
     * doorbell for a queue it is about to destroy while it makes the next. */
    static constexpr uint32_t max_doorbells = 128u;

    DoorbellTable(Platform &platform, MappedRegisterWindow &doorbells);
    DoorbellTable(const DoorbellTable &) = delete;
    DoorbellTable &operator=(const DoorbellTable &) = delete;

    /* A doorbell for `queue` on `page`, answering its handle. invalid_parameter
     * for no queue or a page past the BAR - a page no store could reach -
     * and no_space when the table is full. */
    [[nodiscard]] Status create(HwQueue &queue, uint32_t page, void *&handle);
    /* The kernel address of the doorbell's page and the status a process is to
     * read; invalid_parameter for a handle this table did not make. */
    [[nodiscard]] Status connect(void *handle, void *&address, DoorbellStatus &status);
    [[nodiscard]] Status disconnect(void *handle);
    [[nodiscard]] Status destroy(void *handle);
    /* DestroyHwQueue: a doorbell that outlived its queue would name the page of
     * a context that is gone. */
    void                 forget_queue(const HwQueue *queue);
    /* What a process would read: a handle this table does not know is
     * disconnected for good. */
    DoorbellStatus       status_of(void *handle);
    /* The queue a doorbell hangs off, null for a handle this table did not
     * make: what tells the caller whose context's submissions have moved to
     * user mode. */
    HwQueue             *queue_of(void *handle);
    bool                 serves(const HwQueue *queue);
    uint32_t             live() const;

    /* A line for the table and one per doorbell; `busy=true` and nothing more
     * while another thread holds it. */
    void report(StateWriter &writer);

    /* Every live doorbell names a queue and a page inside the BAR, and no two
     * share a handle. */
    bool invariants_hold() const;

private:
    QueueDoorbell *find(void *handle);

    Platform             &platform_;
    MappedRegisterWindow &doorbells_;
    Mutex                 lock_;
    ObjectPool<QueueDoorbell> held_;

    friend struct DoorbellTableProbe;
};

}  // namespace av

#endif  // AV_DOORBELL_TABLE_HPP
