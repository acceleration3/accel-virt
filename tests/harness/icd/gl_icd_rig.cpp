#include "harness/icd/gl_icd_rig.hpp"

#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/gl_icd_binding.hpp"

namespace av {

namespace {

const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);

}  // namespace

GlIcdRig::GlIcdRig(bool present_thread)
    : platform_(), kmt_(), host_(kmt_), trace_(), lock_(trace_, 'H'), send_lock_(trace_, 'S'), log_(), surface_(),
      threads_(), opengl32_(threads_), present_lock_(trace_, 'P'), present_thread_(), handed_(), shown_(),
      presents_(present_thread_, handed_, shown_, present_lock_, kmt_, surface_, chains_, log_),
      root_(platform_, kmt_, lock_, send_lock_, log_, surface_, threads_, opengl32_, presents_,
            glicd::gl_icd_binding),
      dc_(0), other_dc_(0), memory_dc_(0), window_(0), other_window_(0)
{
    if (!present_thread) presents_.refuse();
    kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
    surface_.add_dc(&dc_, &window_, window_width, window_height);
    surface_.add_dc(&other_dc_, &other_window_, window_width, window_height);
    surface_.add_dc(&memory_dc_, nullptr, 0u, 0u);
}

MockPlatform          &GlIcdRig::platform() { return platform_; }
MockKmt               &GlIcdRig::kmt() { return kmt_; }
ScriptedHost          &GlIcdRig::host() { return host_; }
LockTrace             &GlIcdRig::trace() { return trace_; }
RecordingThreadLock   &GlIcdRig::lock() { return lock_; }
RecordingLog          &GlIcdRig::log() { return log_; }
ScriptedWindowSurface &GlIcdRig::surface() { return surface_; }
ScriptedThreadSlot    &GlIcdRig::threads() { return threads_; }
FakeOpengl32          &GlIcdRig::opengl32() { return opengl32_; }
glicd::GlIcdRoot      &GlIcdRig::root() { return root_; }
glicd::GlPresentLoop  &GlIcdRig::presents() { return presents_; }
ScriptedThread        &GlIcdRig::present_thread() { return present_thread_; }
CountedWakeSignal     &GlIcdRig::handed() { return handed_; }
CountedWakeSignal     &GlIcdRig::shown() { return shown_; }
void                  *GlIcdRig::dc() { return &dc_; }
void                  *GlIcdRig::other_dc() { return &other_dc_; }
void                  *GlIcdRig::memory_dc() { return &memory_dc_; }
void                  *GlIcdRig::window() { return &window_; }

void GlIcdRig::load_with_format()
{
    CHECK(opengl32_.load(glicd::GlIcdRoot::icd_version, 1u) == FakeOpengl32::Load::loaded);
    CHECK(opengl32_.set_pixel_format(dc(), 1));
}

void *GlIcdRig::current_context(const char *renderer, const char *version)
{
    void *const context = opengl32_.create_context(dc());
    if (context == nullptr) return nullptr;
    host_.gl_make(renderer, version);
    /* The root's policy reports a row, so the make asks the host's list. */
    host_.gl_policy(0);
    /* Format 1 is double-buffered D24S8 with no samples. */
    host_.gl_window(1u, 1u);
    return opengl32_.make_current(dc(), context) ? context : nullptr;
}

void *GlIcdRig::current_core_context()
{
    void *const context = opengl32_.create_context(dc());
    if (context == nullptr) return nullptr;
    host_.gl_make_profile("host gpu", "4.6 (Core Profile) Mesa", 4, 6, 1);
    host_.gl_policy(0);
    host_.gl_window(1u, 1u);
    return opengl32_.make_current(dc(), context) ? context : nullptr;
}

}  // namespace av
