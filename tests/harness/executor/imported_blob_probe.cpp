#include "harness/executor/imported_blob_probe.hpp"

namespace av {

ImportedBlobProbe::ImportedBlobProbe(ImportedBlob &target) : blob(target) {}

void ImportedBlobProbe::set_kind(uint32_t kind) { blob.kind_ = kind; }

void ImportedBlobProbe::set_size(uint64_t size) { blob.size_ = size; }

void ImportedBlobProbe::set_pointer(void *pointer) { blob.pointer_ = pointer; }

void ImportedBlobProbe::set_descriptor(int descriptor) { blob.descriptor_ = descriptor; }

void ImportedBlobProbe::set_memory_type(uint32_t type) { blob.memory_type_ = type; }

void ImportedBlobProbe::set_wire_handle(uint64_t handle) { blob.wire_handle_ = handle; }

}  // namespace av
