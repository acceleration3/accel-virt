/* The av scenarios on this machine's GPU (feature 044, T028): the in-guest
 * suite over DdiKmt, the real driver and the real device core, whose executor
 * is the library's own root behind av_executor_ops() - linked into this
 * binary, the table QEMU loads from libav_executor.so - bound to the host GPU
 * that runs the most sub-APIs. The generated clients encode in the guest, the
 * generated server decodes on a device worker and the host's Vulkan, OpenCL
 * and EGL run it. A host with no GPU running a scenario's sub-API has nothing
 * here to judge, and those tests return without checking, as the executor's
 * own GPU tests do. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_suite.hpp"
#include "guest/suite/host_visible_import_scenario.hpp"
#include "guest/suite/isolation_scenario.hpp"
#include "guest/suite/opencl_scenario.hpp"
#include "guest/suite/vulkan_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" const AvExecutorOps *av_executor_ops(void);

/* A real VkDevice, OpenCL context and EGL context allocate from driver modules
 * that never give everything back; named, as the executor's GPU tests name
 * them, rather than leak detection turned off. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n"
           "leak:libvulkan_intel\n"
           "leak:libLLVM\n"
           "leak:libgallium\n";
}

using av::AdapterSearch;
using av::AvgpuAdapters;
using av::FullStackRig;
using av::GuestReport;
using av::GuestSuite;
using av::PciIdentity;
using av::Status;
using av::WidestHostGpu;

namespace {

const PciIdentity av_gpu(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::uint32_t every_sub_api =
    AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL | AV_EXECUTOR_SUB_API_OPENCL;

/* Whether the report holds `line` as a whole line. */
bool reported(const GuestReport &report, const char *line)
{
    const std::size_t length = std::strlen(line);
    for (const char *at = std::strstr(report.text(), line); at != nullptr; at = std::strstr(at + 1, line)) {
        const bool starts = at == report.text() || at[-1] == '\n';
        if (starts && (at[length] == '\n' || at[length] == '\0')) return true;
    }
    return false;
}

void print(const GuestReport &report)
{
    std::fwrite(report.text(), 1u, report.length(), stderr);
}

/* One scenario on the rig, with the adapter found first and closed after, and
 * the rig left with nothing open. */
template <typename Scenario>
void run_alone(FullStackRig &rig, GuestReport &report)
{
    AdapterSearch search(rig.kmt());
    CHECK_STATUS(search.find_avgpu(0u), Status::ok);
    { Scenario scenario(rig.kmt(), report, search);
      scenario.run(); }
    CHECK_STATUS(search.close(), Status::ok);
    if (report.failed() != 0u) print(report);
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    CHECK_EQ(rig.device().device().contexts(), 0u);
}

}  // namespace

/* The Vulkan object chain made on the host GPU and destroyed: every call the
 * guest encoded was decoded and run by the host's Vulkan. */
/* What the device loads is this table, and a null entry in it would be
 * called and crash the device. This is the only binary that links the
 * library's own entry point, so it is the only place the published table can
 * be judged whole. */
TEST(fullstack_av, the_published_table_is_this_abi_and_fills_every_entry)
{ const AvExecutorOps *const ops = av_executor_ops();
  CHECK(ops != nullptr);
  CHECK_EQ(ops->abi_version, std::uint32_t{AV_EXECUTOR_ABI_VERSION});
  CHECK(ops->sub_apis != nullptr);
  CHECK(ops->open_domain != nullptr);
  CHECK(ops->close_domain != nullptr);
  CHECK(ops->allocate_memory != nullptr);
  CHECK(ops->free_memory != nullptr);
  CHECK(ops->create != nullptr);
  CHECK(ops->destroy != nullptr);
  CHECK(ops->execute != nullptr);
  CHECK(ops->end_batch != nullptr);
  CHECK(ops->poll != nullptr);
  CHECK(ops->gpu_progress != nullptr);
  CHECK(ops->set_signal != nullptr);
  CHECK(ops->import_blob != nullptr);
  CHECK(ops->release_blob != nullptr);
  CHECK(ops->imported_handle != nullptr);
  CHECK(ops->share_object != nullptr);
  CHECK(ops->open_object != nullptr);
  CHECK(ops->close_object != nullptr);
  CHECK(ops->release_shared != nullptr);
  CHECK(ops->read_image != nullptr);
  CHECK(ops->export_image != nullptr);
  CHECK(ops->map_frame != nullptr);
  CHECK(ops->unmap_frame != nullptr);
  CHECK(ops->export_surface != nullptr);
  CHECK(ops->sync_surface != nullptr);
  CHECK(ops->answer_part != nullptr); }

TEST(fullstack_av, the_vulkan_chain_runs_on_the_host_gpu)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_VULKAN) == 0u) return;
  FullStackRig rig(*av_executor_ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  run_alone<av::VulkanScenario>(rig, report);
  CHECK_EQ(report.failed(), 0u);
  /* One, whatever this host has: the guest is shown the GPU the device is
   * bound to and no other (T034). */
  CHECK(reported(report, "ok vulkan.physical_device 1"));
  CHECK(reported(report, "ok vulkan.bind"));
  CHECK(reported(report, "ok vulkan.destroy")); }

/* Vector add on the host GPU, the output read back exact. */
TEST(fullstack_av, vector_add_runs_on_the_host_gpu)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  FullStackRig rig(*av_executor_ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  run_alone<av::OpenClScenario>(rig, report);
  CHECK_EQ(report.failed(), 0u);
  /* The bound GPU's platform alone, where this host has three (T034). */
  CHECK(reported(report, "ok opencl.platform 1"));
  CHECK(reported(report, "ok opencl.sum 64"));
  CHECK(reported(report, "ok opencl.release")); }

/* Feature 045's experiment through the real driver, device and executor: two
 * 64 MiB host-visible chunks of the GPU's own memory (docs/host-memory.md),
 * opened by escape in the Vulkan type the guest chose and imported with
 * IMPORT_BLOB into the host GPU's device, the source's pattern copied into
 * the destination by a submitted vkCmdCopyBuffer, and read back exactly
 * through the guest's view - the guest saw what the host GPU wrote. */
TEST(fullstack_av, the_host_gpu_copies_between_two_imported_guest_chunks)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_VULKAN) == 0u) return;
  FullStackRig rig(*av_executor_ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  run_alone<av::HostVisibleImportScenario>(rig, report);
  CHECK_EQ(report.failed(), 0u);
  CHECK(reported(report, "ok import.wait 1"));
  CHECK(reported(report, "ok import.copied 1048576"));
  CHECK(reported(report, "ok import.guest_saw_host_write 0"));
  CHECK(reported(report, "ok import.release"));
  CHECK_EQ(rig.device().device().blobs(), 0u); }

/* The host refuses the truncated create-info before any ICD sees it, the
 * driver completes that context's work with no reply and refuses its next
 * submission - never DMA_FAULTED, which the fake dxgkrnl fails as the real one
 * bugchecks on it - and the host's Vulkan keeps serving the other context and
 * a new one. */
TEST(fullstack_av, a_malformed_stream_faults_only_its_own_context)
{ const WidestHostGpu gpu;
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_VULKAN) == 0u) return;
  FullStackRig rig(*av_executor_ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  run_alone<av::IsolationScenario>(rig, report);
  CHECK_EQ(report.failed(), 0u);
  CHECK(reported(report, "ok isolation.fault"));
  CHECK(reported(report, "ok isolation.relink"));
  CHECK(rig.fdo().started()); }

/* What `avtest.exe av` runs in the guest, whole: every step passes. */
TEST(fullstack_av, the_av_run_passes_every_step_on_the_host_gpu)
{ const WidestHostGpu gpu;
  if (gpu.sub_apis() != every_sub_api) return;
  FullStackRig rig(*av_executor_ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  /* With hardware scheduling on, the run submits through the process's own
   * doorbell wherever it can, which is what the guest does on a machine that
   * has it (feature 049). */
  rig.kmt().allow_hardware_scheduling();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  std::uint32_t failed = 0u;
  { GuestSuite suite(rig.kmt(), report, 0u);
    failed = suite.run_av(); }
  if (failed != 0u) print(report);
  CHECK_EQ(failed, 0u);
  CHECK(reported(report, "avtest: 85 passed, 0 failed"));
  /* The benchmark measured both paths on the one boot, on the real GPU. */
  CHECK(reported(report, "note submit.doorbell connected"));
  CHECK(std::strstr(report.text(), "\nok submit.ratio ") != nullptr);
  CHECK(reported(report, "ok import.copied 1048576"));
  CHECK(reported(report, "ok transport.nop 0"));
  CHECK(reported(report, "ok vulkan.destroy"));
  CHECK(reported(report, "ok opencl.sum 64"));
  CHECK(reported(report, "ok opengl.nop 0"));
  CHECK(reported(report, "ok isolation.relink"));
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }
