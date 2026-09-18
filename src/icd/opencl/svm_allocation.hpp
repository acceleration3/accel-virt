/* One coarse-grained shared virtual memory allocation.
 *
 * Two pieces of memory, one allocation. The host's clSVMAlloc answered an
 * address in the host process, which is the only kind of pointer a kernel may
 * dereference; the guest cannot touch that, so the application is handed a
 * mirror of the same size in its own address space and the two are
 * synchronised at map and unmap.
 *
 * That is not a compromise, it is what coarse-grained means: host access to a
 * coarse-grained buffer is defined only between clEnqueueSVMMap and
 * clEnqueueSVMUnmap, so there is no moment at which the application is
 * entitled to see the two disagree. */
#ifndef AV_CLICD_SVM_ALLOCATION_HPP
#define AV_CLICD_SVM_ALLOCATION_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class SvmAllocation {
public:
    SvmAllocation();

    /* Holds the pair. The mirror is the application's, the host address is
     * what crosses the wire. */
    void take(uint64_t host_base, uint8_t *mirror, size_t bytes);
    void give_up();
    bool held() const;

    uint64_t host_base() const;
    uint8_t *mirror() const;
    size_t   bytes() const;

    /* Whether `address` is inside the mirror, and the host address of the
     * same byte; 0 for one that is not. An address inside is enough: a kernel
     * argument may point into the middle of an allocation. */
    bool     covers(const void *address) const;
    uint64_t host_address_of(const void *address) const;

    /* clEnqueueSVMMap and clEnqueueSVMUnmap nest, as the API says they do. */
    void     mapped();
    /* False when nothing was mapped. */
    bool     unmapped();
    uint32_t map_depth() const;

    bool invariants_hold() const;

private:
    uint64_t host_base_;
    uint8_t *mirror_;
    size_t   bytes_;
    uint32_t map_depth_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_SVM_ALLOCATION_HPP
