/* The private driver data of a shareable allocation: an AllocationDescriptor
 * followed by the SharedObjectRecord carrying the shared object's identity
 * (contracts/shared_objects.md). A creator hands the kernel these bytes
 * holding an unassigned record; the miniport mints an id into the record at
 * the creating device's own open, and dxgkrnl hands the creator's bytes to
 * every later opener. Every site that makes, mints into or reads such data -
 * the miniport, the Direct3D surfaces, the Vulkan carrier, the shared-handle
 * tables and the window chains - holds it as one of these.
 *
 * Bytes another party wrote - the kernel minting into lend(), another
 * process's creator - are unread until decode() or reread() has judged them;
 * nothing about them is answered before. */
#ifndef AV_SHARED_ALLOCATION_DATA_HPP
#define AV_SHARED_ALLOCATION_DATA_HPP

#include "core/support/status.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedAllocationData {
public:
    static constexpr size_t encoded_size = AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size;

    /* Nothing yet: unread. */
    SharedAllocationData();
    /* What a creator hands the kernel: `descriptor`, then an unassigned record
     * of `kind` and `size`. */
    SharedAllocationData(const AllocationDescriptor &descriptor, const SharedObjectKind &kind, uint64_t size);

    /* Its encoded_size bytes, as held. */
    const uint8_t *bytes() const;
    /* Its bytes lent to the kernel to mint into or to copy another process's
     * into: unread from now until reread(). */
    uint8_t       *lend();
    /* The bytes lent, `written` of them, judged as the descriptor and record
     * they must be: malformed for any length but encoded_size, and whatever
     * either part refuses. Unread unless ok. */
    [[nodiscard]] Status reread(size_t written);
    /* `length` bytes another party wrote, copied and judged as reread()
     * judges. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* `minted` in place of the record held, as the creating device's open
     * writes it back for dxgkrnl to keep: invalid_state while unread. */
    [[nodiscard]] Status mint(const SharedObjectRecord &minted);

    bool                        read() const;
    /* Only once read. */
    const AllocationDescriptor &descriptor() const;
    const SharedObjectRecord   &record() const;
    /* Whether an id has been minted into it: false while unread. */
    bool                        assigned() const;
    /* The minted id; 0 while unread or unassigned. */
    uint64_t                    object_id() const;
    /* The allocation's bytes, as its descriptor says; 0 while unread. */
    uint64_t                    size() const;

    /* Read, the descriptor and record held encode to the bytes held. */
    bool invariants_hold() const;

private:
    uint8_t              bytes_[encoded_size];
    AllocationDescriptor descriptor_;
    SharedObjectRecord   record_;
    bool                 read_;
};

}  // namespace av

#endif  // AV_SHARED_ALLOCATION_DATA_HPP
