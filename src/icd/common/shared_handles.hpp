#ifndef AV_ICD_SHARED_HANDLES_HPP
#define AV_ICD_SHARED_HANDLES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
class ThreadLock;
}

namespace av::icd {

struct SharedHandlesProbe;

struct SharedHandleSlot {
    uint64_t handle_;
    uint64_t object_id_;
    uint32_t device_;
    uint32_t resource_;
};

class SharedHandles {
public:
    static constexpr uint32_t max_private = 128u;

    SharedHandles(Kmt &kmt, ThreadLock &lock, BlockSource &memory);
    SharedHandles(const SharedHandles &) = delete;
    SharedHandles &operator=(const SharedHandles &) = delete;

    [[nodiscard]] Status mint(uint32_t device, uint32_t carrier, uint64_t object_id, uint64_t &handle);
    [[nodiscard]] Status adopt(uint32_t device, uint64_t handle, uint64_t &object_id);
    bool                 resolve(uint64_t handle, uint64_t &object_id) const;
    void                 forget(uint64_t handle);
    void                 forget_device(uint32_t device);

    uint32_t live() const;

    bool invariants_hold() const;

private:
    friend struct SharedHandlesProbe;

    [[nodiscard]] Status make_room();
    void                 keep(const SharedHandleSlot &row);
    void                 give_back(const SharedHandleSlot &row);

    Kmt                                    &kmt_;
    ThreadLock                             &lock_;
    KeyedSlots<uint64_t, SharedHandleSlot> slots_;
    KeyedSlots<uint64_t, uint64_t>         handles_of_;
};

}

#endif
