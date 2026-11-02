#ifndef AV_GL_ICD_RIG_HPP
#define AV_GL_ICD_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/fake_opengl32.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/gl_present_loop.hpp"

#include <cstdint>

namespace av {

class GlIcdRig {
public:
    static constexpr std::uint32_t window_width = 64u;
    static constexpr std::uint32_t window_height = 48u;

    explicit GlIcdRig(bool present_thread = false);
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
    glicd::GlPresentLoop  &presents();
    ScriptedThread        &present_thread();
    CountedWakeSignal     &handed();
    CountedWakeSignal     &shown();

    void *dc();
    void *window();
    void *other_dc();
    void *memory_dc();

    void  load_with_format();
    void *current_context(const char *renderer = "host gpu", const char *version = "4.6.0 host");
    void *current_core_context();

private:
    MockPlatform          platform_;
    MockKmt               kmt_;
    ScriptedHost          host_;
    LockTrace             trace_;
    RecordingThreadLock   lock_;
    RecordingThreadLock   send_lock_;
    RecordingLog          log_;
    ScriptedWindowSurface surface_;
    ScriptedWindowChains  chains_;
    ScriptedThreadSlot    threads_;
    FakeOpengl32          opengl32_;
    RecordingThreadLock   present_lock_;
    ScriptedThread        present_thread_;
    CountedWakeSignal     handed_;
    CountedWakeSignal     shown_;
    glicd::GlPresentLoop  presents_;
    glicd::GlIcdRoot      root_;
    int                   dc_;
    int                   other_dc_;
    int                   memory_dc_;
    int                   window_;
    int                   other_window_;
};

}

#endif
