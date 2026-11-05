#include "harness/executor/imported_blob_probe.hpp"

namespace av {

ImportedBlobProbe::ImportedBlobProbe(ImportedBlob &target) : blob(target) {}

void ImportedBlobProbe::set_id(uint64_t id) { blob.id_ = id; }

void ImportedBlobProbe::forget_memory() { blob.memory_ = nullptr; }

void ImportedBlobProbe::set_handle(uint64_t handle) { blob.handle_ = handle; }

void ImportedBlobProbe::record(uint64_t handle) { blob.record(handle); }

}
