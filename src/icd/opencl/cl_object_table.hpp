/* Every object this ICD has handed the application, and the only place a
 * pointer from outside is believed.
 *
 * An application passes back whatever it likes: an object it already released,
 * one of another kind, a stack address, a number it made up. None of those may
 * be dereferenced to find out - so nothing is read through the pointer at all.
 * The table searches what it made for that address, which is the same thing
 * the Vulkan ICD's registry does and for the same reason.
 *
 * An object holds what it was made on, as OpenCL's reference counting says: a
 * context whose application let go of it lives on, unreachable, while a buffer
 * made on it does, and goes with the last of them - so a guest that releases a
 * context while holding a buffer keeps a buffer whose context is there. */
#ifndef AV_CLICD_CL_OBJECT_TABLE_HPP
#define AV_CLICD_CL_OBJECT_TABLE_HPP

#include "core/support/object_pool.hpp"
#include "icd/opencl/cl_object.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::clicd {

struct ClObjectTableProbe;

class ClObjectTable {
public:
    ClObjectTable(BlockSource &memory, const cl_icd_dispatch &table);
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
    /* Destroys `object` and its whole subtree, whatever its count. */
    void destroy(ClObject &object);

    uint32_t live() const;
    /* How many of `kind` are live, for the tests and a state dump. */
    uint32_t live_of(ClObject::Kind kind) const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct ClObjectTableProbe;

    ObjectPool<ClObject>   pool_;
    const cl_icd_dispatch &table_;
    uint32_t               live_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_OBJECT_TABLE_HPP
