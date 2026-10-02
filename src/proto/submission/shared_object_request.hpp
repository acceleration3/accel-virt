/* The body the three shared-object transport commands share (docs/protocol.md,
 * Transport commands; feature 053, contracts/wire_additions.md).
 *
 * One shape for SHARE_OBJECT, OPEN_SHARED_OBJECT and RELEASE_SHARED_OBJECT,
 * because all three name one object of one kind and the only difference is
 * whether a wire handle travels with it. Which of them may set `wire_handle`
 * is the command's rule and not the body's, so it is carried here unjudged:
 * a body that judged it would have to know which command it was inside. */
#ifndef AV_SHARED_OBJECT_REQUEST_HPP
#define AV_SHARED_OBJECT_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedObjectRequest {
public:
    static constexpr size_t   encoded_size = 32u;
    /* Memory is shared in whole pages, as every other length here is. */
    static constexpr uint64_t page_size = SharedObjectKind::page_size;

    SharedObjectRequest();
    SharedObjectRequest(uint64_t object_id, const SharedObjectKind &kind, uint64_t wire_handle,
                        uint64_t size);

    uint64_t                object_id() const;
    const SharedObjectKind &kind() const;
    uint64_t                wire_handle() const;
    /* The bytes the object is, which a memory import needs and a fence has
     * none of: whole pages and non-zero for memory, 0 for either fence. */
    uint64_t                size() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any length but encoded_size - framing, which no producer
     * writes; invalid_parameter for an object id of 0, a kind this protocol
     * does not have, a non-zero reserved field, or a size that is not what its
     * kind has - a fence carries none, and memory carries whole pages.
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint64_t         object_id_;
    SharedObjectKind kind_;
    uint64_t         wire_handle_;
    uint64_t         size_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_REQUEST_HPP
