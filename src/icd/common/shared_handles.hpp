/* The NT handles this process holds for shared objects, and the ids behind
 * them (feature 053, US1; research.md §9).
 *
 * 047's handles were numbers this ICD made up: they named a host object inside
 * one process and meant nothing outside it. These are kernel objects. One is
 * minted by the kernel for an allocation this process created, or adopted from
 * a handle another process duplicated in, and either way the id behind it came
 * out of the *creating* process's allocation private driver data - the only
 * thing dxgkrnl carries across that boundary (docs/kb/04-memory.md).
 *
 * A handle this table did not mint and did not adopt resolves to nothing,
 * which is what stops a guest naming an object it never opened.
 *
 * All three ICDs hold one of these, because all three resolve a handle the
 * same way. The lock is its own: an import happens on the application's
 * thread, and nothing it does reaches back into the root. */
#ifndef AV_ICD_SHARED_HANDLES_HPP
#define AV_ICD_SHARED_HANDLES_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::icd {

class ThreadLock;
struct SharedHandlesProbe;

/* One handle this process holds. A type of its own so the table's slots are
 * complete (tools/header_scan.py reads a class's members only to its first
 * nested type). */
struct SharedHandleSlot {
    uint64_t handle_;
    uint64_t object_id_;
    /* The device the handle was minted on or adopted into. */
    uint32_t device_;
    /* The resource an adopt opened, which this table destroys because nothing
     * else knows it exists; 0 for a handle this process minted, whose carrier
     * is the application's. */
    uint32_t resource_;
};

class SharedHandles {
public:
    /* More than an application shares: DXGI interop is a handful of surfaces
     * and fences per device, as 047's table is sized for. */
    static constexpr uint32_t max_handles = 64u;
    /* The allocation private driver data an open carries back: a descriptor
     * and a shared object's record, with room over. */
    static constexpr uint32_t max_private = 128u;

    SharedHandles(Kmt &kmt, ThreadLock &lock);
    SharedHandles(const SharedHandles &) = delete;
    SharedHandles &operator=(const SharedHandles &) = delete;

    /* An NT handle for the resource `carrier` of `device`, which already
     * carries `object_id` - the id this process's own create was handed back.
     * The same object minted twice answers the same handle, as Windows does
     * for a handle to one object. invalid_parameter for an id of 0; no_space
     * when every row is taken; whatever the kernel answered when it would not
     * share. 0 unless ok. */
    [[nodiscard]] Status mint(uint32_t device, uint32_t carrier, uint64_t object_id, uint64_t &handle);
    /* A handle another process sent, opened on `device`, with the id read out
     * of the creating process's own bytes. A handle already held answers its
     * id and opens nothing again. invalid_parameter for a handle of 0 or bytes
     * that carry no assigned record; not_found for a handle this process may
     * not open; no_space when every row is taken. 0 unless ok. */
    [[nodiscard]] Status adopt(uint32_t device, uint64_t handle, uint64_t &object_id);
    /* The object `handle` names. False for a handle this table neither minted
     * nor adopted, which leaves `object_id` alone. */
    bool                 resolve(uint64_t handle, uint64_t &object_id) const;
    /* The handle closed, and what an adopt opened for it destroyed. Nothing
     * for a handle this table does not hold. */
    void                 forget(uint64_t handle);
    /* Every handle of `device`, forgotten: a device destroyed takes what its
     * process held with it. */
    void                 forget_device(uint32_t device);

    uint32_t live() const;

    bool invariants_hold() const;

private:
    friend struct SharedHandlesProbe;

    /* The row `handle` is in, or null. The caller holds the lock. */
    SharedHandleSlot       *row_of(uint64_t handle);
    const SharedHandleSlot *row_of(uint64_t handle) const;
    /* The row `object_id` is in, or null. The caller holds the lock. */
    SharedHandleSlot       *row_for(uint64_t object_id);
    /* The row dropped, its handle closed and the resource an adopt opened for
     * it destroyed. The caller holds the lock. */
    void                    give_back(SharedHandleSlot &row);

    Kmt              &kmt_;
    ThreadLock       &lock_;
    SharedHandleSlot  slots_[max_handles];
    uint32_t          live_;
};

}  // namespace av::icd

#endif  // AV_ICD_SHARED_HANDLES_HPP
