#include "device/abi/av_executor.h"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/loader_vulkan.hpp"
#include "guest/probe/vulkan_probe.hpp"
#include "harness/guest/directory_image_sink.hpp"
#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/icd/bound_icd_entry.hpp"
#include "core/log/keyed_log.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/icd/surface_window_frame.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libglapi\n"
           "leak:libvulkan\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:libvulkan_intel\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libz\n"
           "leak:libLLVM\n"
           "leak:libgallium\n"
           "leak:libav_executor\n";
}

using av::DdiKmt;
using av::ExecutorLibraryFile;
using av::FullStackRig;
using av::GuestReport;
using av::LoaderVulkan;
using av::LockTrace;
using av::MockPlatform;
using av::RecordingLog;
using av::RecordingThreadLock;
using av::VulkanProbe;
using av::WidestHostGpu;

namespace {
constexpr std::uint32_t window_side = av::VulkanProbe::extent;

class IcdProcess {
public:
    explicit IcdProcess(DdiKmt &kmt)
        : lock_(trace_, 'H'), send_lock_(trace_, 'S'), memory_lock_(trace_, 'M'), present_lock_(trace_, 'P'), lines_(memory_, log_, "avvk: "),
          root_(memory_, kmt, lock_, send_lock_, memory_lock_, lines_, av::icd::icd_binding, windows_, chains_,
                present_thread_, handed_, shown_, present_lock_, wake_thread_)
    {
        root_.presentation().refuse_presents();
    }

    av::icd::IcdRoot &root() { return root_; }
    RecordingLog     &log() { return log_; }
    av::ScriptedWindowSurface &windows() { return windows_; }

private:
    MockPlatform        memory_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingThreadLock send_lock_;
    RecordingThreadLock memory_lock_;
    RecordingThreadLock       present_lock_;
    RecordingLog        log_;
    av::KeyedLog              lines_;
    av::ScriptedWindowSurface windows_;
    av::ScriptedWindowChains  chains_;
    av::ScriptedThread        present_thread_;
    av::ScriptedThread        wake_thread_;
    av::CountedWakeSignal     handed_;
    av::CountedWakeSignal     shown_;
    av::icd::IcdRoot    root_;
};

bool reported(const GuestReport &report, const char *line)
{
    const std::size_t length = std::strlen(line);
    for (const char *at = std::strstr(report.text(), line); at != nullptr; at = std::strstr(at + 1, line)) {
        const bool starts = at == report.text() || at[-1] == '\n';
        if (starts && (at[length] == '\n' || at[length] == '\0')) return true;
    }
    return false;
}

}

TEST(fullstack_vulkan_icd, the_probe_passes_every_step_through_the_icd)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_VULKAN) == 0u) return;
  const ExecutorLibraryFile library(AV_EXECUTOR_LIBRARY);
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory, "avvkprobe");
  { IcdProcess process(rig.kmt());
    LoaderVulkan vulkan(&av::bound_icd_instance_proc_addr);
    av::DirectoryImageSink images(AV_IMAGE_DIR);
    void *const window = reinterpret_cast<void *>(0x7770u);
    process.windows().add_dc(reinterpret_cast<void *>(0x7771u), window, window_side, window_side);
    av::SurfaceWindowFrame frames(process.windows(), window, window_side, window_side);
    { VulkanProbe probe(rig.kmt(), report, vulkan, images, frames, reinterpret_cast<void *>(0x7772u), window);
      probe.run(); }
    CHECK(process.root().invariants_hold());
    for (std::uint32_t index = 0u; report.failed() != 0u && index < process.log().lines(); index++)
        std::fprintf(stderr, "%s\n", process.log().line(index)); }
  report.finish();
  if (report.failed() != 0u) std::fwrite(report.text(), 1u, report.length(), stderr);
  CHECK(reported(report, "ok vulkan.instance"));
  CHECK(reported(report, "ok vulkan.adapter_luid 256"));
  CHECK(reported(report, "ok vulkan.physical_device 256"));
  CHECK(reported(report, "ok vulkan.device"));
  CHECK(reported(report, "ok vulkan.compute 256"));
  CHECK(reported(report, "ok vulkan.draw 4096"));
  CHECK(reported(report, "ok vulkan.lost"));
  CHECK(reported(report, "ok vulkan.recreated 256"));
  CHECK(reported(report, "ok vulkan.surface 4096"));
  CHECK(reported(report, "ok vulkan.present 4096"));
  CHECK(reported(report, "ok vulkan.resized 9216"));
  CHECK(reported(report, "avvkprobe: 12 passed, 0 failed"));
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }
