/* A UmdRoot as a Direct3D runtime finds it, over MockKmt with the rvkgpu
 * adapter present under UmdRig::adapter_luid. The root's lock records its
 * order in a trace and its log is kept, exactly as IcdRig does for the Vulkan
 * ICD, so a reader moving between the three user-mode drivers reads the same
 * rig. The root binds itself for the rig's life, so the DDI table thunks reach
 * it. */
#ifndef AV_UMD_RIG_HPP
#define AV_UMD_RIG_HPP

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
    /* Both halves non-zero and different, so a half dropped or swapped shows. */
    static constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;
    static constexpr std::uint64_t local_budget = 512ull * 1024ull * 1024ull;
    static constexpr std::uint64_t non_local_budget = 3ull * 1024ull * 1024ull * 1024ull;

    /* `builder` is the host driver the root hands devices to; none refuses them. */
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
    d3d::UmdRoot        root_;
};

}  // namespace av

#endif  // AV_UMD_RIG_HPP
