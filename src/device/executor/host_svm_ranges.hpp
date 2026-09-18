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
 * dereferenced; addresses are compared, and the comparison cannot wrap. */
#ifndef AV_HOST_SVM_RANGES_HPP
#define AV_HOST_SVM_RANGES_HPP

#include <stdint.h>

namespace av {

struct HostSvmRangesProbe;

class HostSvmRanges {
public:
    /* A process that holds more shared allocations than this has had enough;
     * the guest is refused at the allocation, where it can be told, rather
     * than later at a kernel argument, where it cannot. */
    static constexpr uint32_t max_ranges = 256u;

    HostSvmRanges();
    HostSvmRanges(const HostSvmRanges &) = delete;
    HostSvmRanges &operator=(const HostSvmRanges &) = delete;

    /* Holds what clSVMAlloc just answered. False for null, for no bytes at
     * all, for a range that would run past the end of the address space, and
     * when full. The same base twice is held once, with the newer size: an
     * allocator may hand back an address it freed. */
    [[nodiscard]] bool remember(void *base, uint64_t bytes);
    /* What clSVMFree was given. False for an address that is not the base of
     * a live range - an address inside one is not what was allocated. */
    bool               forget(void *base);
    void               forget_all();

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

    /* The slot whose base is `address`, or `count_` for none. */
    uint32_t slot_of(uint64_t address) const;

    /* One slot is the same index in both. */
    uint64_t bases_[max_ranges];
    uint64_t sizes_[max_ranges];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_HOST_SVM_RANGES_HPP
