/* A GlIcdRoot as opengl32 finds it, over MockKmt with the rvkgpu adapter
 * present: the host played through ScriptedHost, the root's and the send
 * lock's order in one trace, a window and a memory DC declared, threads played
 * by name, and FakeOpengl32 driving the ICD's exports and standing in for the
 * system opengl32. The root binds itself for the rig's life. */
#ifndef AV_GL_ICD_RIG_HPP
#define AV_GL_ICD_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/fake_opengl32.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/gl_icd_root.hpp"

#include <cstdint>

namespace av {

class GlIcdRig {
public:
    static constexpr std::uint32_t window_width = 64u;
    static constexpr std::uint32_t window_height = 48u;

    GlIcdRig();
    GlIcdRig(const GlIcdRig &) = delete;
    GlIcdRig &operator=(const GlIcdRig &) = delete;

    MockPlatform          &platform();
    MockKmt               &kmt();
    ScriptedHost          &host();
    LockTrace             &trace();
    RecordingThreadLock   &lock();
    RecordingLog          &log();
    ScriptedWindowSurface &surface();
    ScriptedThreadSlot    &threads();
    FakeOpengl32          &opengl32();
    glicd::GlIcdRoot      &root();

    /* A DC on a window, a second DC on another window, and a memory DC. */
    void *dc();
    void *other_dc();
    void *memory_dc();

    /* opengl32 loaded the ICD and set format 1 on dc(). */
    void  load_with_format();
    /* A context on dc() made current on the calling thread, the host
     * answering the make; null when that failed. */
    void *current_context(const char *renderer = "host gpu", const char *version = "4.6.0 host");

private:
    MockPlatform          platform_;
    MockKmt               kmt_;
    ScriptedHost          host_;
    LockTrace             trace_;
    RecordingThreadLock   lock_;
    RecordingThreadLock   send_lock_;
    RecordingLog          log_;
    ScriptedWindowSurface surface_;
    ScriptedThreadSlot    threads_;
    FakeOpengl32          opengl32_;
    glicd::GlIcdRoot      root_;
    int                   dc_;
    int                   other_dc_;
    int                   memory_dc_;
    int                   window_;
    int                   other_window_;
};

}  // namespace av

#endif  // AV_GL_ICD_RIG_HPP
