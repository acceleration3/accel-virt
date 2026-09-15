/* Reaches into an ImportedBlob so invariant tests can corrupt it. Declared a
 * friend by ImportedBlob; nothing outside the tests uses it. */
#ifndef AV_IMPORTED_BLOB_PROBE_HPP
#define AV_IMPORTED_BLOB_PROBE_HPP

#include "device/executor/imported_blob.hpp"

#include <stdint.h>

namespace av {

struct ImportedBlobProbe {
    explicit ImportedBlobProbe(ImportedBlob &target);

    void set_kind(uint32_t kind);
    void set_size(uint64_t size);
    void set_pointer(void *pointer);
    void set_descriptor(int descriptor);
    void set_memory_type(uint32_t type);
    void set_wire_handle(uint64_t handle);

    ImportedBlob &blob;
};

}  // namespace av

#endif  // AV_IMPORTED_BLOB_PROBE_HPP
