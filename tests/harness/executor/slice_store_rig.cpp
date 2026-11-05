#include "harness/executor/slice_store_rig.hpp"

#include "device/abi/av_executor.h"
#include "executor/source_memory.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <unistd.h>

namespace av {

SliceStoreRig::SliceStoreRig()
    : platform_(), exporter_(), source_(exporter_, platform_), sink_(), lines_(sink_, "avtest: "), held_{}
{
    exporter_.add_type(MockMemoryExporter::host_visible | MockMemoryExporter::host_coherent, true);
}

SliceStoreRig::~SliceStoreRig()
{
    for (SourceMemory *&memory : held_) {
        if (memory == nullptr) continue;
        const std::uint64_t key = memory->key();
        source_.release(*memory);
        source_.free(domain, key);
        memory = nullptr;
    }
}

SourceMemory *SliceStoreRig::store(std::uint64_t key)
{
    AvHostMemory made = {-1, 0u, 0u};
    CHECK_STATUS(source_.allocate(domain, key, 4096u, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), made),
                 Status::ok);
    if (made.fd >= 0) ::close(made.fd);
    SourceMemory *const memory = source_.acquire(domain, key);
    CHECK(memory != nullptr);
    for (SourceMemory *&slot : held_) {
        if (slot != nullptr) continue;
        slot = memory;
        break;
    }
    return memory;
}

void SliceStoreRig::drop(SourceMemory &store)
{
    for (SourceMemory *&slot : held_) {
        if (slot != &store) continue;
        const std::uint64_t key = store.key();
        source_.release(store);
        source_.free(domain, key);
        slot = nullptr;
        return;
    }
}

MockPlatform     &SliceStoreRig::platform() { return platform_; }
HostMemorySource &SliceStoreRig::source() { return source_; }
RecordingLog     &SliceStoreRig::sink() { return sink_; }
DriverLog        &SliceStoreRig::lines() { return lines_; }

}
