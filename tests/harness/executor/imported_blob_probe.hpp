/* Reaches into an ImportedBlob so invariant tests can corrupt it: a free slot
 * left with an id, an object or a wire handle of an import it no longer
 * holds. Declared a friend by ImportedBlob; nothing outside the tests uses
 * it. */
#ifndef AV_IMPORTED_BLOB_PROBE_HPP
#define AV_IMPORTED_BLOB_PROBE_HPP

#include "executor/imported_blob.hpp"

#include <stdint.h>

namespace av {

struct ImportedBlobProbe {
    explicit ImportedBlobProbe(ImportedBlob &target);

    void set_id(uint64_t id);
    void set_object(uint64_t object);
    void set_wire_handle(uint64_t handle);

    ImportedBlob &blob;
};

}  // namespace av

#endif  // AV_IMPORTED_BLOB_PROBE_HPP
