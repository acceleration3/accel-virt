#ifndef AV_SLICE_STORE_RIG_HPP
#define AV_SLICE_STORE_RIG_HPP

#include "core/log/driver_log.hpp"
#include "executor/host_memory_source.hpp"
#include "harness/executor/mock_memory_exporter.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class SourceMemory;

class SliceStoreRig {
public:
    static constexpr std::uint32_t domain = 1u;

    SliceStoreRig();
    ~SliceStoreRig();
    SliceStoreRig(const SliceStoreRig &) = delete;
    SliceStoreRig &operator=(const SliceStoreRig &) = delete;

    SourceMemory *store(std::uint64_t key);
    void          drop(SourceMemory &store);

    MockPlatform     &platform();
    HostMemorySource &source();
    RecordingLog     &sink();
    DriverLog        &lines();

private:
    static constexpr std::uint32_t max_held = 16u;

    MockPlatform       platform_;
    MockMemoryExporter exporter_;
    HostMemorySource   source_;
    RecordingLog       sink_;
    DriverLog          lines_;
    SourceMemory      *held_[max_held];
};

}

#endif
