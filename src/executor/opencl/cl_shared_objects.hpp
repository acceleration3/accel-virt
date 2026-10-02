/* The shared objects one OpenCL context opened, and the three host objects
 * behind each (feature 053, T019).
 *
 * This host's OpenCL cannot take a descriptor - it has no
 * `cl_khr_external_memory` - but it can name an OpenGL object, and OpenGL
 * takes the descriptor. So an open makes three things in order: a GL memory
 * object from the descriptor, a GL buffer over that memory, and the `cl_mem`
 * OpenCL made from the buffer. The guest names the last of them and knows
 * nothing of the other two.
 *
 * A row is what keeps the three together. A release that freed the `cl_mem`
 * and left the rest would leak the allocation for as long as the device lives,
 * and one that deleted the memory object first would pull the storage out from
 * under a live `cl_mem`, so the order is the reverse of the making and the row
 * is what makes it possible. */
#ifndef AV_CL_SHARED_OBJECTS_HPP
#define AV_CL_SHARED_OBJECTS_HPP

#include <stdint.h>

namespace av {

struct ClSharedObjectsProbe;

class ClSharedObjects {
public:
    /* One guest process's shared buffers, with the room the device's own
     * per-context table has. */
    static constexpr uint32_t max_objects = 64u;

    ClSharedObjects();
    ClSharedObjects(const ClSharedObjects &) = delete;
    ClSharedObjects &operator=(const ClSharedObjects &) = delete;

    /* Remembers the three objects one open made. False, holding nothing, for
     * any of them missing and for a `cl_mem` this context already holds - the
     * host answering one number for two objects is a table nothing could
     * unpick. */
    [[nodiscard]] bool remember(uint64_t mem, uint32_t buffer, uint32_t memory);
    /* The row `mem` names, dropped, with the two GL names to delete after the
     * `cl_mem` goes. False, and both untouched, for one this context did not
     * open - which is what stops a guest releasing a number it chose. */
    [[nodiscard]] bool forget(uint64_t mem, uint32_t &buffer, uint32_t &memory);
    /* One row still held, dropped: what a teardown destroys them all through. */
    [[nodiscard]] bool take_any(uint64_t &mem, uint32_t &buffer, uint32_t &memory);

    uint32_t count() const;

    /* A live row names all three, and no `cl_mem` appears twice. */
    bool invariants_hold() const;

private:
    friend struct ClSharedObjectsProbe;

    uint64_t mems_[max_objects];
    uint32_t buffers_[max_objects];
    uint32_t memories_[max_objects];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_CL_SHARED_OBJECTS_HPP
