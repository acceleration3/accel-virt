#ifndef AV_IMPORTED_BLOB_PROBE_HPP
#define AV_IMPORTED_BLOB_PROBE_HPP

#include "executor/imported_blob.hpp"

#include <stdint.h>

namespace av {

struct ImportedBlobProbe {
    explicit ImportedBlobProbe(ImportedBlob &target);

    void set_id(uint64_t id);
    void forget_memory();
    void set_handle(uint64_t handle);
    void record(uint64_t handle);

    ImportedBlob &blob;
};

}

#endif
