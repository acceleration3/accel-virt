#ifndef AV_UMD_RIG_HPP
#define AV_UMD_RIG_HPP

#include "core/log/driver_log.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstdint>

namespace av {

class UmdRig {
public:
    static constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;
    static constexpr std::uint64_t local_budget = 512ull * 1024ull * 1024ull;
    static constexpr std::uint64_t non_local_budget = 3ull * 1024ull * 1024ull * 1024ull;

    explicit UmdRig(d3d::UmdHostDriver *builder = nullptr);
    UmdRig(const UmdRig &) = delete;
    UmdRig &operator=(const UmdRig &) = delete;

    MockPlatform        &platform();
    MockKmt             &kmt();
    LockTrace           &trace();
    RecordingThreadLock &lock();
    RecordingLog        &log();
    d3d::UmdRoot        &root();

private:
    MockPlatform        platform_;
    MockKmt             kmt_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingLog        log_;
    DriverLog           lines_;
    d3d::UmdRoot        root_;
};

}

#endif
