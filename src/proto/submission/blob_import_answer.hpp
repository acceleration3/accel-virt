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
        /* No resource, or a reserved field that is not zero. */
        refused_request = 1u,
        /* Not a host blob opened for this context. */
        unknown_resource = 2u,
        /* No executor, or one whose API will not take the memory. */
        executor_refused = 3u,
        /* No host memory to keep the import in. */
        no_room = 4u,
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
