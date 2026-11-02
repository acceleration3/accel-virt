#include "harness/executor/imported_blob_probe.hpp"

namespace av {

ImportedBlobProbe::ImportedBlobProbe(ImportedBlob &target) : blob(target) {}

void ImportedBlobProbe::set_id(uint64_t id) { blob.id_ = id; }

void ImportedBlobProbe::set_object(uint64_t object) { blob.object_ = object; }

void ImportedBlobProbe::set_wire_handle(uint64_t handle) { blob.wire_handle_ = handle; }

}
