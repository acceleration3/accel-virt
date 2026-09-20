/* The adapter's shared objects, and the ids it mints (feature 053).
 *
 * kb 11's sixth risk says device object identity must be global rather than
 * per open. This is the guest half of that: one table per adapter, whose ids
 * the miniport writes into an allocation's private driver data at create and
 * reads back out of it at open - in another process, on another device, with
 * every other handle in sight different.
 *
 * It believes nothing it did not write. An id it never minted, an id from
 * another adapter, and a kind or size that disagrees with the object it holds
 * are each refused with nothing on the host touched, which is what US1's
 * fourth scenario asks for. */
#ifndef AV_SHARED_OBJECT_TABLE_HPP
#define AV_SHARED_OBJECT_TABLE_HPP

#include "core/memory/shared_object.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Allocation;
class SharedObjectRecord;
class StateWriter;
struct SharedObjectTableProbe;

class SharedObjectTable {
public:
    explicit SharedObjectTable(Platform &platform);
    SharedObjectTable(const SharedObjectTable &) = delete;
    SharedObjectTable &operator=(const SharedObjectTable &) = delete;

    /* StartDevice: the number this adapter puts in every record it mints, so a
     * record from another adapter of this kernel is one this one refuses. Taken
     * from the performance counter, which is what an adapter has: a miniport is
     * never told its LUID. Done once; invalid_state afterwards. */
    [[nodiscard]] Status initialize();
    uint64_t             adapter() const;

    /* CreateAllocation: an identity for `allocation`, written into `record`.
     * invalid_parameter for a record that already carries one - a creator
     * names the kind and the size and neither of the other two; no_memory when
     * the pool cannot grow. Nothing is minted unless ok.
     *
     * The size the record declares is the *object's*, which need not be the
     * allocation's: an ICD sharing host memory has no guest allocation of it
     * and creates a one-page carrier for the identity alone (feature 053). */
    [[nodiscard]] Status mint(Allocation &allocation, SharedObjectRecord &record);
    /* OpenAllocation: the object `record` names, with one more device holding
     * it. not_found for an id this adapter never minted and for one from
     * another adapter; invalid_parameter for a record carrying no id at all,
     * or whose kind or size disagrees with the object; the object's answer at
     * its ceiling. Nothing is held unless ok. */
    [[nodiscard]] Status open(const SharedObjectRecord &record, SharedObject *&found);
    /* CloseAllocation: one fewer device. invalid_parameter for an object this
     * table does not hold, and for one nobody holds. */
    [[nodiscard]] Status close(SharedObject &object);
    /* DestroyAllocation: the identity goes with the allocation it named, so a
     * later record naming that id finds nothing. */
    void                 forget(const Allocation &allocation);
    uint32_t             live() const;

    /* A line for the table and one per object; `busy=true` and nothing more
     * while another thread holds it. */
    void report(StateWriter &writer);

    /* Every live object's id is below the next one to be minted, no two share
     * an id, and each is one an allocation still names. */
    bool invariants_hold() const;

private:
    /* The object `id` names, or null. The caller holds the mutex. */
    SharedObject *find(uint64_t id);

    Platform                 &platform_;
    Mutex                     mutex_;
    ObjectPool<SharedObject>  objects_;
    /* Only ever increases, so an id is never reused while this adapter runs:
     * a process that kept a stale record must not find someone else's object. */
    uint64_t                  next_id_;
    uint64_t                  adapter_;

    friend struct SharedObjectTableProbe;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_TABLE_HPP
