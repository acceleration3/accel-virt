/* One process's OpenCL ICD, as the Khronos loader meets it: the root over
 * MockKmt with the rvkgpu adapter, the host played through ScriptedHost, the
 * locks' trace, the log, and a loader that dispatches through the table rather
 * than calling the root by name.
 *
 * The rig reaches the ICD only through what the loader reaches, which is the
 * point: a test that called the root's methods directly would not notice a
 * dispatch table slot left null. */
#ifndef AV_CL_ICD_RIG_HPP
#define AV_CL_ICD_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"

namespace av {

class ClIcdRig {
public:
    /* Both halves non-zero and different, so a half dropped or swapped
     * shows. */
    static constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;

    ClIcdRig();
    ClIcdRig(const ClIcdRig &) = delete;
    ClIcdRig &operator=(const ClIcdRig &) = delete;

    MockPlatform        &platform();
    MockKmt             &kmt();
    ScriptedHost        &host();
    LockTrace           &trace();
    RecordingThreadLock &lock();
    RecordingThreadLock &send_lock();
    RecordingLog        &log();
    clicd::ClIcdRoot    &root();
    FakeClLoader        &loader();

private:
    MockPlatform         platform_;
    MockKmt              kmt_;
    ScriptedHost         host_;
    LockTrace            trace_;
    RecordingThreadLock  lock_;
    RecordingThreadLock  send_lock_;
    RecordingLog         log_;
    /* The root binds itself to the process's one binding - the same one the
     * dispatch table's entries reach it through - so what a test drives is
     * what a loader would. One rig at a time, as one ICD per process. */
    clicd::ClIcdRoot     root_;
    FakeClLoader         loader_;
};

}  // namespace av

#endif  // AV_CL_ICD_RIG_HPP
