/* A host device lost under the whole guest stack, on this machine's GPU: the
 * in-guest HostLossScenario over DdiKmt, the real driver, the real device
 * core and the executor library's own root, whose Direct3D 11 device is
 * DXVK's - lost by the layer that loses a Vulkan device on request
 * (tests/gpu_fault), which this test alone loads.
 *
 * What it proves is what the guest sees, end to end: the context that drew is
 * refused at once, the idle one - DWM on an unchanging desktop - is told by
 * its fence page within a moment and why (fault 7), a Vulkan context on a
 * device of its own runs on, and a Direct3D context made afterwards runs on
 * the device the host made again. A host without DXVK or without the layer
 * returns without judging. */
#include "device/abi/av_executor.h"
#include "guest/kmt/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/host_loss_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" const AvExecutorOps *av_executor_ops(void);

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libdxvk\n"
           "leak:libvkd3d\n"
           "leak:libav_device_loss_layer\n"
           "leak:libSDL3\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n"
           "leak:libLLVM\n"
           "leak:libgallium\n";
}

namespace {

/* The line `line` holds whole in the report, or the one starting with
 * `start` when the rest is a measurement: its value, else ~0. */
bool reported(const av::GuestReport &report, const char *line)
{
    const std::size_t length = std::strlen(line);
    for (const char *at = std::strstr(report.text(), line); at != nullptr; at = std::strstr(at + 1, line)) {
        const bool starts = at == report.text() || at[-1] == '\n';
        if (starts && (at[length] == '\n' || at[length] == '\0' || at[length] == ' ')) return true;
    }
    return false;
}

}  // namespace

TEST(fullstack_device_loss, every_direct3d_context_is_removed_others_run_on_and_a_new_one_draws)
{ if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
  const av::WidestHostGpu gpu;
  if ((gpu.sub_apis() & (AV_EXECUTOR_SUB_API_D3D | AV_EXECUTOR_SUB_API_VULKAN)) !=
      (AV_EXECUTOR_SUB_API_D3D | AV_EXECUTOR_SUB_API_VULKAN)) {
      std::fprintf(stderr, "no GPU here runs both Direct3D and Vulkan: nothing judged\n");
      return;
  }
  av::FullStackRig rig(*av_executor_ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  av::MockPlatform report_memory;
  av::GuestReport report(report_memory);
  { av::AdapterSearch search(rig.kmt());
    CHECK_STATUS(search.find_avgpu(0u), av::Status::ok);
    { av::HostLossScenario scenario(rig.kmt(), report, search);
      scenario.run(); }
    CHECK_STATUS(search.close(), av::Status::ok); }
  std::fwrite(report.text(), 1u, report.length(), stderr);
  if (std::strstr(report.text(), "note host_loss.lose") != nullptr) {
      std::fprintf(stderr, "the device-loss layer is not loaded: nothing judged\n");
      return;
  }
  CHECK_EQ(report.failed(), 0u);
  CHECK(reported(report, "ok host_loss.drawn"));
  CHECK(reported(report, "ok host_loss.lose"));
  CHECK(reported(report, "ok host_loss.drawing_removed"));
  CHECK(reported(report, "ok host_loss.idle_removed"));
  CHECK(reported(report, "ok host_loss.idle_code 7"));
  CHECK(reported(report, "ok host_loss.idle_refused"));
  CHECK(reported(report, "ok host_loss.other_kept"));
  CHECK(reported(report, "ok host_loss.recreated"));
  /* Everything the scenario opened, the faulted contexts included, is gone. */
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }
