#ifndef AV_ADOPTED_BLOBS_PROBE_HPP
#define AV_ADOPTED_BLOBS_PROBE_HPP

#include "executor/vulkan/adopted_blobs.hpp"

#include <stdint.h>

namespace av {

class ImportedBlob;

struct AdoptedBlobsProbe {
    explicit AdoptedBlobsProbe(AdoptedBlobs &target);

    void set_count(uint32_t count);
    void set_blob(uint32_t index, ImportedBlob *blob);

    AdoptedBlobs &adopted;
};

}

#endif
