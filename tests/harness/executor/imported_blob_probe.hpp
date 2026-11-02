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

}

#endif
