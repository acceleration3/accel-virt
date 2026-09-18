/* The native GPU fences dxgkrnl has made on this adapter (WDDM 3.1, feature
 * 049).
 *
 * A native fence is two 64-bit values in memory the OS allocated: the current
 * value, which says how far work has got, and the monitored value, which says
 * what a waiter is waiting for. The OS owns the memory and hands the driver
 * the kernel address of each value when it wants one written; what the driver
 * owns is the object itself - which fences exist, and what was last written
 * into each - so that a handle it never gave is refused rather than followed
 * (constitution V).
 *
 * The device signals nothing here: this adapter's work is finished on a
 * context's ring, and what a process waits on for that is the hardware queue's
 * progress fence (research.md §12). What this table is for is the capability
 * the OS demands before it will hand out a doorbell at all (§14), and the
 * updates it makes through it.
 */
#ifndef AV_NATIVE_FENCE_TABLE_HPP
#define AV_NATIVE_FENCE_TABLE_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargCreateNativeFenceRecord;
struct DxgkargDestroyNativeFenceRecord;
struct DxgkargUpdateNativeFenceValuesRecord;
class StateWriter;

struct NativeFenceTableProbe;

class NativeFenceTable {
public:
    /* What a process may hold at once; past this the OS is told there is no
     * room rather than being given a fence nobody tracks. */
    static constexpr uint32_t max_fences = 256u;

    explicit NativeFenceTable(Platform &platform);
    NativeFenceTable(const NativeFenceTable &) = delete;
    NativeFenceTable &operator=(const NativeFenceTable &) = delete;

    /* PASSIVE. The driver's handle for the fence dxgkrnl is making, which is
     * the slot it was written into. invalid_parameter for no record or a type
     * this driver has no meaning for; no_space when the table is full. */
    [[nodiscard]] Status create(DxgkargCreateNativeFenceRecord &request);
    [[nodiscard]] Status destroy(const DxgkargDestroyNativeFenceRecord &request);
    /* PROFILE and DISPATCH respectively, as the DDIs declare - so neither
     * takes a lock: each writes the value the OS asked for at the address the
     * OS named, for every fence this table made. invalid_parameter for a
     * malformed request or a handle this table did not give, with nothing
     * written for any of them. */
    [[nodiscard]] Status update_monitored(const DxgkargUpdateNativeFenceValuesRecord &request);
    [[nodiscard]] Status update_current(const DxgkargUpdateNativeFenceValuesRecord &request);

    /* Read without the lock, as the updates are: the slots are fixed storage
     * and each value is written whole. */
    uint32_t live() const;
    /* What was last written into a fence, for a handle this table gave: how a
     * test sees what the OS asked for. */
    uint64_t monitored_value(const void *handle) const;
    uint64_t current_value(const void *handle) const;

    /* A line for the table and one per fence; `busy=true` and nothing more
     * while another thread holds it. */
    void report(StateWriter &writer);

    /* Every live fence has a handle of its own, and a fence nobody made holds
     * no values. */
    bool invariants_hold() const;

private:
    /* The handle a slot is known by, and the slot a handle names: a fence
     * handle is its slot plus one, so no live fence is ever the null handle. */
    void    *handle_of(uint32_t slot) const;
    uint32_t slot_of(const void *handle) const;
    /* Both updates, which differ only in which of a fence's two values they
     * write - so which one is the parameter. */
    [[nodiscard]] Status update(const DxgkargUpdateNativeFenceValuesRecord &request, uint64_t (&values)[max_fences]);

    Platform &platform_;
    Mutex     lock_;
    /* Which fences exist, what each was made as, and the two values the OS
     * last asked to be written into it. */
    bool      live_slots_[max_fences];
    uint32_t  types_[max_fences];
    uint64_t  monitored_[max_fences];
    uint64_t  current_[max_fences];
    uint32_t  live_;

    friend struct NativeFenceTableProbe;
};

}  // namespace av

#endif  // AV_NATIVE_FENCE_TABLE_HPP
