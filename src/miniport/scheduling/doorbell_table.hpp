#ifndef AV_DOORBELL_TABLE_HPP
#define AV_DOORBELL_TABLE_HPP

#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "miniport/scheduling/queue_doorbell.hpp"
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
    DoorbellTable(Platform &platform, MappedRegisterWindow &doorbells);
    DoorbellTable(const DoorbellTable &) = delete;
    DoorbellTable &operator=(const DoorbellTable &) = delete;

    [[nodiscard]] Status create(HwQueue &queue, uint32_t page, void *&handle);
    [[nodiscard]] Status connect(void *handle, void *&address, DoorbellStatus &status, HwQueue *&queue);
    [[nodiscard]] Status disconnect(void *handle);
    [[nodiscard]] Status destroy(void *handle);
    void                 forget_queue(const HwQueue *queue);
    DoorbellStatus       status_of(void *handle);
    HwQueue             *queue_of(void *handle);
    bool                 serves(const HwQueue *queue);
    uint32_t             live() const;

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    QueueDoorbell *find(void *handle);

    Platform             &platform_;
    MappedRegisterWindow &doorbells_;
    Mutex                 lock_;
    ObjectPool<QueueDoorbell> held_;

    friend struct DoorbellTableProbe;
};

}

#endif
