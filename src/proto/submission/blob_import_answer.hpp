/* What IMPORT_BLOB answers (docs/protocol.md, Transport commands; feature
 * 045): whether the import was made and, when it was, the wire handle of the
 * device memory the guest names it by from then on. A refusal is an answer,
 * not a fault: the context keeps running. */
#ifndef AV_BLOB_IMPORT_ANSWER_HPP
#define AV_BLOB_IMPORT_ANSWER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImportAnswer {
public:
    static constexpr size_t encoded_size = 16u;

    enum : uint32_t {
        imported = 0u,
        /* A reserved field, an offset or size that is not whole pages, no size,
         * or a range past 2^64. */
        refused_request = 1u,
        /* Not a guest blob in one of the context's chunk slots. */
        unknown_resource = 2u,
        outside_blob = 3u,
        /* The host cannot make one range of the blob's pages. */
        unmappable_pages = 4u,
        /* No executor, a library that cannot name an import, or a refusal. */
        executor_refused = 5u,
        /* Every import the context may hold, or no host memory to hold them in. */
        no_room = 6u,
    };

    BlobImportAnswer();
    BlobImportAnswer(uint32_t status, uint64_t device_memory);

    uint32_t status() const;
    uint64_t device_memory() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a short buffer, a non-zero reserved field, a status past
     * no_room, an import with no handle or a refusal with one. Unchanged
     * unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    uint32_t status_;
    uint64_t device_memory_;
};

}  // namespace av

#endif  // AV_BLOB_IMPORT_ANSWER_HPP
