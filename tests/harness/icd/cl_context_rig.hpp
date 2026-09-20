/* One process's OpenCL device link and the pieces a GuestClContext is made
 * over, for the tests of the context classes below ClIcdRoot: MockKmt with the
 * rvkgpu adapter, the host played through ScriptedHost, the send lock's trace
 * and the log.
 *
 * The OpenGL rig's twin, minus the extension policy: an OpenCL context asks
 * the host nothing at a make, because its record carries no options and the
 * device it is made on is the one the adapter names. */
#ifndef AV_CL_CONTEXT_RIG_HPP
#define AV_CL_CONTEXT_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/shared_handles.hpp"

namespace av {

class ClContextRig {
public:
    /* The adapter, the device and its paging queue. */
    static constexpr std::uint32_t link_objects = 3u;

    ClContextRig();
    ClContextRig(const ClContextRig &) = delete;
    ClContextRig &operator=(const ClContextRig &) = delete;

    MockPlatform           &platform();
    MockKmt                &kmt();
    ScriptedHost           &host();
    LockTrace              &trace();
    RecordingThreadLock    &send_lock();
    RecordingLog           &log();
    av::icd::IcdConnection &connection();
    /* The NT handles a context's imports resolve through (feature 053). */
    av::icd::SharedHandles &handles();

private:
    MockPlatform           platform_;
    MockKmt                kmt_;
    ScriptedHost           host_;
    LockTrace              trace_;
    RecordingThreadLock    send_lock_;
    RecordingLog           log_;
    av::icd::IcdConnection connection_;
    RecordingThreadLock    handles_lock_;
    av::icd::SharedHandles handles_;
};

}  // namespace av

#endif  // AV_CL_CONTEXT_RIG_HPP
