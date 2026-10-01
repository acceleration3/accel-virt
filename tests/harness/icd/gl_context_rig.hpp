/* One process's OpenGL device link and the pieces a GuestGlContext is made
 * over, for the tests of the context classes below GlIcdRoot: MockKmt with
 * the avgpu adapter, the host played through ScriptedHost, the send lock's
 * trace and the log. */
#ifndef AV_GL_CONTEXT_RIG_HPP
#define AV_GL_CONTEXT_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opengl/gl_extension_policy.hpp"

namespace av {

class GlContextRig {
public:
    /* The adapter, the device and its paging queue. */
    static constexpr std::uint32_t link_objects = 3u;
    /* How far the guest's clock moves each time the ICD reads it: a tenth of a
     * millisecond, so a spin for a reply gives up after ten reads. */
    static constexpr std::uint64_t transport_clock_step = 1000u;

    GlContextRig();
    GlContextRig(const GlContextRig &) = delete;
    GlContextRig &operator=(const GlContextRig &) = delete;

    MockPlatform         &platform();
    MockKmt              &kmt();
    ScriptedHost         &host();
    LockTrace            &trace();
    RecordingThreadLock  &send_lock();
    RecordingLog         &log();
    av::icd::IcdConnection  &connection();
    /* A policy reporting nothing, which asks the host nothing at a make. */
    const glicd::GlExtensionPolicy &extensions() const;
    /* The NT handles a context's imports resolve through (feature 053). */
    av::icd::SharedHandles &handles();

private:
    MockPlatform        platform_;
    MockKmt             kmt_;
    ScriptedHost        host_;
    LockTrace           trace_;
    RecordingThreadLock send_lock_;
    RecordingLog        log_;
    av::icd::IcdConnection connection_;
    glicd::GlExtensionPolicy extensions_;
    RecordingThreadLock    handles_lock_;
    av::icd::SharedHandles handles_;
};

}  // namespace av

#endif  // AV_GL_CONTEXT_RIG_HPP
