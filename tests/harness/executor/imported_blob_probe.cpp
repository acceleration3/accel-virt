#include "harness/executor/imported_blob_probe.hpp"

namespace av {

ImportedBlobProbe::ImportedBlobProbe(ImportedBlob &target) : blob(target) {}

void ImportedBlobProbe::set_kind(uint32_t kind) { blob.kind_ = kind; }

void ImportedBlobProbe::set_size(uint64_t size) { blob.size_ = size; }

void ImportedBlobProbe::set_pointer(void *pointer) { blob.pointer_ = pointer; }

void ImportedBlobProbe::set_descriptor(int descriptor) { blob.descriptor_ = descriptor; }

}  // namespace av
