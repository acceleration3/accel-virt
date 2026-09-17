/* One process's OpenGL device link and the pieces a GuestGlContext is made
 * over, for the tests of the context classes below GlIcdRoot: MockKmt with
 * the rvkgpu adapter, the host played through ScriptedHost, the send lock's
 * trace and the log. */
#ifndef AV_GL_CONTEXT_RIG_HPP
#define AV_GL_CONTEXT_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/gl_connection.hpp"
#include "icd/opengl/gl_extension_policy.hpp"

namespace av {

class GlContextRig {
public:
    /* The adapter, the device and its paging queue. */
    static constexpr std::uint32_t link_objects = 3u;

    GlContextRig();
    GlContextRig(const GlContextRig &) = delete;
    GlContextRig &operator=(const GlContextRig &) = delete;

    MockPlatform         &platform();
    MockKmt              &kmt();
    ScriptedHost         &host();
    LockTrace            &trace();
    RecordingThreadLock  &send_lock();
    RecordingLog         &log();
    glicd::GlConnection  &connection();
    /* A policy reporting nothing, which asks the host nothing at a make. */
    const glicd::GlExtensionPolicy &extensions() const;

private:
    MockPlatform        platform_;
    MockKmt             kmt_;
    ScriptedHost        host_;
    LockTrace           trace_;
    RecordingThreadLock send_lock_;
    RecordingLog        log_;
    glicd::GlConnection connection_;
    glicd::GlExtensionPolicy extensions_;
};

}  // namespace av

#endif  // AV_GL_CONTEXT_RIG_HPP
