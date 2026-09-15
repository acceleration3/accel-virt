/* Reaches into an AdoptedBlobs so invariant tests can corrupt it. Declared a
 * friend by AdoptedBlobs; nothing outside the tests uses it. */
#ifndef AV_ADOPTED_BLOBS_PROBE_HPP
#define AV_ADOPTED_BLOBS_PROBE_HPP

#include "device/executor/adopted_blobs.hpp"

#include <stdint.h>

namespace av {

class ImportedBlob;

struct AdoptedBlobsProbe {
    explicit AdoptedBlobsProbe(AdoptedBlobs &target);

    void set_count(uint32_t count);
    void set_blob(uint32_t index, ImportedBlob *blob);

    AdoptedBlobs &adopted;
};

}  // namespace av

#endif  // AV_ADOPTED_BLOBS_PROBE_HPP
