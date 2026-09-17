/* rvkvkprobe's proof through the ICD on this machine's GPU (feature 045,
 * T038): the probe calls the ICD's own entry point as the Windows loader would
 * - vkCreateInstance from the null instance, everything else through the
 * instance - and the ICD root, over DdiKmt, carries every command through the
 * real driver and device core to the executor library, loaded from its file
 * and bound to the host GPU that runs the most sub-APIs. Host-visible memory
 * is the guest process's own pages, imported by the host GPU. A host without
 * Vulkan has nothing here to judge, and the test returns without checking. */
#include "device/abi/rvk_executor.h"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/loader_vulkan.hpp"
#include "guest/suite/vulkan_probe.hpp"
#include "harness/guest/directory_image_sink.hpp"
#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/icd/bound_icd_entry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

/* The executor library opens every host API when it loads, and a real
 * VkDevice, OpenCL platform and EGL display allocate from driver modules that
 * never give everything back; named, as the other full-stack tests name them,
 * rather than leak detection turned off. */
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
           "leak:librvk_executor\n";
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

/* The ICD as one guest process holds it: its own memory, locks and log. */
class IcdProcess {
public:
    explicit IcdProcess(DdiKmt &kmt)
        : lock_(trace_, 'H'), send_lock_(trace_, 'S'), memory_lock_(trace_, 'M'),
          root_(memory_, kmt, lock_, send_lock_, memory_lock_, log_, av::icd::icd_binding)
    {
    }

    av::icd::IcdRoot &root() { return root_; }
    RecordingLog     &log() { return log_; }

private:
    MockPlatform        memory_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingThreadLock send_lock_;
    RecordingThreadLock memory_lock_;
    RecordingLog        log_;
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

}  // namespace

TEST(fullstack_vulkan_icd, the_probe_passes_every_step_through_the_icd)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & RVK_EXECUTOR_SUB_API_VULKAN) == 0u) return;
  const ExecutorLibraryFile library(AV_EXECUTOR_LIBRARY);
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  GuestReport report("rvkvkprobe");
  { IcdProcess process(rig.kmt());
    LoaderVulkan vulkan(&av::bound_icd_instance_proc_addr);
    av::DirectoryImageSink images(AV_IMAGE_DIR);
    { VulkanProbe probe(rig.kmt(), report, vulkan, images);
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
  CHECK(reported(report, "rvkvkprobe: 8 passed, 0 failed"));
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }
