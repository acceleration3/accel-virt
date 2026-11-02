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
        refused_request = 1u,
        unknown_resource = 2u,
        executor_refused = 3u,
        no_room = 4u,
    };

    BlobImportAnswer();
    BlobImportAnswer(uint32_t status, uint64_t device_memory);

    uint32_t status() const;
    uint64_t device_memory() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    uint32_t status_;
    uint64_t device_memory_;
};

}

#endif
