/* Every object this ICD has handed the application, and the only place a
 * pointer from outside is believed.
 *
 * An application passes back whatever it likes: an object it already released,
 * one of another kind, a stack address, a number it made up. None of those may
 * be dereferenced to find out - so nothing is read through the pointer at all.
 * The table searches what it made for that address, as the Vulkan ICD's
 * registry does: both are a HandleTree.
 *
 * Every entry point looks its arguments up here, on whatever thread the
 * application calls from, while another may be making or releasing objects -
 * which grows and frees the pool's chunks under a search. So the table takes
 * a lock of its own around everything it does; what it answers is the
 * application's to keep alive while it uses it, as OpenCL's counting says.
 *
 * An object holds what it was made on, as OpenCL's reference counting says: a
 * context whose application let go of it lives on, unreachable, while a buffer
 * made on it does, and goes with the last of them - so a guest that releases a
 * context while holding a buffer keeps a buffer whose context is there. */
#ifndef AV_CLICD_CL_OBJECT_TABLE_HPP
#define AV_CLICD_CL_OBJECT_TABLE_HPP

#include "icd/common/handle_tree.hpp"
#include "icd/opencl/cl_object.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::clicd {

struct ClObjectTableProbe;

class ClObjectTable {
public:
    ClObjectTable(BlockSource &memory, const cl_icd_dispatch &table, icd::ThreadLock &lock);
    ClObjectTable(const ClObjectTable &) = delete;
    ClObjectTable &operator=(const ClObjectTable &) = delete;

    /* Holds one more object of `kind`, answered by the host under `wire`, on
     * `context` and under `parent`. no_memory when there is no room;
     * invalid_parameter for wire 0, for a parent of a kind that is not lower
     * than this one, and for a parent this table does not hold. */
    [[nodiscard]] Status make(ClObject::Kind kind, uint64_t wire, GuestClContext *context,
                              ClObject *parent, ClObject *&object);

    /* The object `pointer` names, or null: for a pointer this table never
     * made, for one it has destroyed, for one the application let go of that
     * lives on only for what was made on it, and for one of another kind.
     * Nothing is read through `pointer` unless it is one of this table's own. */
    ClObject *made(const void *pointer, ClObject::Kind kind) const;

    /* One reference of the application's given back. When that was the last
     * and nothing made on the object lives, it is destroyed, and so is each
     * object above it that was held only by it; `freed` is the context of a
     * context object that went, which its owner then tears down. False for an
     * object this does not hold or that the application had let go of. */
    bool release(ClObject &object, GuestClContext *&freed);
    /* The same, saying which objects went: the first `room` of them into
     * `gone`, in the order they went, and how many into `count`. The
     * addresses are the application's handles for objects that no longer
     * exist, which only a destructor callback is handed. */
    bool release(ClObject &object, GuestClContext *&freed, const void **gone, uint32_t room, uint32_t &count);
    /* Destroys `object` and its whole subtree, whatever its count. */
    void destroy(ClObject &object);

    uint32_t live() const;
    /* How many of `kind` are live, for the tests and a state dump. */
    uint32_t live_of(ClObject::Kind kind) const;
    /* Whether every live command queue of `context` is known to have
     * finished what was sent to it (ClObject::known_idle). */
    bool     queues_idle(const GuestClContext *context) const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct ClObjectTableProbe;

    /* `object` and its subtree destroyed; the caller holds the lock. */
    void destroy_held(ClObject &object);

    BlockSource              &memory_;
    icd::HandleTree<ClObject> tree_;
    const cl_icd_dispatch    &table_;
    icd::ThreadLock          &lock_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_OBJECT_TABLE_HPP
