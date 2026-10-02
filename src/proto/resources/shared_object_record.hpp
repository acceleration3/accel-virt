/* One shared object's identity, as it travels in an allocation's private
 * driver data (feature 053, data-model.md, contracts/shared_objects.md).
 *
 * This is the only channel that crosses a process boundary without a name:
 * dxgkrnl hands the opening process the *creating* process's private data
 * (docs/kb/04-memory.md, "Allocation lifetime traps"), so the id the miniport
 * mints at create is what the second process's open reads back. Everything in
 * it came from user mode, on both sides of that journey.
 *
 * A record arrives from a creator with `object_id` and `adapter_id` both 0 -
 * a creator knows the kind and the size of what it is sharing and can know
 * neither of the other two - and the miniport writes both back into it. A
 * record with zeros is therefore decodable and is not openable, which
 * `assigned` is the question for. */
#ifndef AV_SHARED_OBJECT_RECORD_HPP
#define AV_SHARED_OBJECT_RECORD_HPP

#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedObjectRecord {
public:
    static constexpr size_t   encoded_size = 40u;
    /* The size a shared object is measured in, as every other page count here. */
    static constexpr uint64_t page_size = SharedObjectKind::page_size;

    SharedObjectRecord();
    SharedObjectRecord(uint64_t object_id, const SharedObjectKind &kind, uint64_t adapter_id, uint64_t size);

    uint64_t                object_id() const;
    const SharedObjectKind &kind() const;
    /* The adapter that minted the id, which is what makes a record from
     * another adapter refusable (spec, Edge Cases). It is the adapter's own
     * number rather than its LUID: a miniport is never told its LUID, and what
     * this has to tell apart is two adapters of one running kernel. */
    uint64_t                adapter_id() const;
    uint64_t                size() const;
    /* Whether an id has been minted into it, which is what an open needs and a
     * create must not have. */
    bool                    assigned() const;
    /* What the miniport minted, written back so the runtime keeps it and the
     * opening process reads it. */
    void                    assign(uint64_t object_id, uint64_t adapter_id);

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any length but encoded_size; invalid_parameter for a magic
     * that is not this record's, a version this build does not know, a kind it
     * does not know, a non-zero reserved field, an id without an adapter or an
     * adapter without an id, or a size that is not its kind's - whole non-zero
     * pages for memory, and none at all for either fence, which is the rule
     * the wire body keeps. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00535641u;   /* "AVS" */
    static constexpr uint32_t version = 1u;

    bool fields_valid() const;

    uint64_t         object_id_;
    SharedObjectKind kind_;
    uint64_t         adapter_id_;
    uint64_t         size_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_RECORD_HPP
