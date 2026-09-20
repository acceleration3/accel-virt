/* The shared virtual memory one context has allocated, and the only address
 * translation in this design.
 *
 * The application holds mirrors; the wire carries host addresses. Every
 * pointer an application hands back - a kernel argument, the source of a copy,
 * the range to map - is turned into the host address of the same byte here,
 * and a pointer that is inside no live allocation is refused before anything
 * is sent. The device bounds-checks it again when it arrives, because a guest
 * is not to be believed either.
 *
 * The mirrors come from the process heap and are this table's to free. */
#ifndef AV_CLICD_SVM_TABLE_HPP
#define AV_CLICD_SVM_TABLE_HPP

#include "icd/opencl/svm_allocation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::clicd {

struct SvmTableProbe;

class SvmTable {
public:
    /* A context holding more shared allocations than this has had enough; the
     * refusal lands on clSVMAlloc, which an application can see. */
    static constexpr uint32_t max_allocations = 256u;

    explicit SvmTable(BlockSource &memory);
    ~SvmTable();
    SvmTable(const SvmTable &) = delete;
    SvmTable &operator=(const SvmTable &) = delete;

    /* Takes a mirror for what the host allocated at `host_base`, and answers
     * it - what the application is handed. Null when there is no room, when
     * the mirror cannot be had, for a host base of zero, and for no bytes at
     * all. */
    void *take(uint64_t host_base, size_t bytes);
    /* Gives back what the application passed to clSVMFree, answering the host
     * address to free; 0 for a pointer that is not the base of a live
     * allocation - an address inside one is not what was allocated. */
    uint64_t give_back(const void *mirror);
    /* Frees every mirror; the host's own go with the context. */
    void     give_back_all();

    /* The host address of the same byte, or 0 for a pointer inside no live
     * allocation. This is the translation; nothing else does it. */
    uint64_t host_address_of(const void *address) const;
    /* The allocation `address` is inside, or null: what a map needs, because
     * a map has bytes to copy as well as an address to translate. */
    SvmAllocation       *covering(const void *address);
    const SvmAllocation *covering(const void *address) const;

    uint32_t live() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct SvmTableProbe;

    BlockSource  &memory_;
    SvmAllocation allocations_[max_allocations];
    uint32_t      live_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_SVM_TABLE_HPP
