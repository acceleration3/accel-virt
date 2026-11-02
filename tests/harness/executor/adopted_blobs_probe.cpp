#include "harness/executor/adopted_blobs_probe.hpp"

namespace av {

AdoptedBlobsProbe::AdoptedBlobsProbe(AdoptedBlobs &target) : adopted(target) {}

void AdoptedBlobsProbe::set_count(uint32_t count) { adopted.count_ = count; }

void AdoptedBlobsProbe::set_blob(uint32_t index, ImportedBlob *blob) { adopted.blobs_[index] = blob; }

}
