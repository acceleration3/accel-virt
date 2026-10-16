/* The shared virtual memory one session's guest allocated, as addresses.
 *
 * A guest that holds SVM hands its addresses back - as a kernel argument, as
 * the source of a copy, as the range to map - and each is a number it could
 * have invented. This is what the decoder asks before the host's OpenCL is
 * given a pointer at all (clproto::ServerDispatch::svm_for): whether the
 * address, together with what the command will read or write from it, lies
 * inside something this session allocated.
 *
 * Unlike a handle an address need not name the start of one, because a kernel
 * argument may point into the middle of an allocation. Nothing here is ever
 * dereferenced; addresses are compared, and the comparison cannot wrap.
 *
 * Kept sorted by base, so a lookup is a binary search and a walk back over
 * the ranges that could still reach the address: ranges may overlap - two
 * maps of one buffer - so the walk stops only once a base is further below
 * the address than the widest range held is long. */
#ifndef AV_HOST_SVM_RANGES_HPP
#define AV_HOST_SVM_RANGES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
struct HostSvmRangesProbe;

class HostSvmRanges {
public:
    explicit HostSvmRanges(BlockSource &blocks);
    HostSvmRanges(const HostSvmRanges &) = delete;
    HostSvmRanges &operator=(const HostSvmRanges &) = delete;

    /* Room for one more range, so the remember after an allocation or a map
     * cannot fail for want of memory: what a caller asks before it makes
     * something it would have to undo. no_memory when there is none. */
    [[nodiscard]] Status make_room();
    /* Holds what clSVMAlloc or a map just answered, for `owner` (the mapped
     * buffer, or 0). invalid_parameter for null, for no bytes at all and for
     * a range that would run past the end of the address space; no_memory
     * when the table cannot grow. The same base twice is held once, with the
     * newer size: an allocator may hand back an address it freed. */
    [[nodiscard]] Status remember(void *base, uint64_t bytes, uint64_t owner);
    /* Forgets every range remembered for `owner` - a buffer mapped and then
     * released, whose mapped addresses nothing names again. Answers how many;
     * owner 0 is no owner, and forgets none. */
    uint32_t             forget_owned(uint64_t owner);
    /* What clSVMFree was given. False for an address that is not the base of
     * a live range - an address inside one is not what was allocated. */
    bool                 forget(void *base);
    void                 forget_all();

    /* `address` as a pointer when it and the `bytes` after it lie inside one
     * live range, and null otherwise: for an address in no range, for one
     * whose extent leaves its range, and for arithmetic that would wrap. Zero
     * bytes still has to be inside a range, because an address one past the
     * end is not a place a command may name. */
    void    *resolve(uint64_t address, uint64_t bytes) const;
    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct HostSvmRangesProbe;

    struct Range {
        uint64_t base_;
        uint64_t size_;
        uint64_t owner_;
    };

    /* The index of the first range whose base is above `address`. */
    uint32_t above(uint64_t address) const;

    GrowableArray<Range> ranges_;
    /* No range held is longer: what bounds the walk back from a lookup. It
     * only shrinks when the table empties. */
    uint64_t             widest_;
};

}  // namespace av

#endif  // AV_HOST_SVM_RANGES_HPP
