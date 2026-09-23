/* The executor library's state dump (docs/trace.md): the GPUs the host
 * offered and each live session's API and GPU, written without waiting. */
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/executor_root.hpp"
#include "harness/executor/executor_root_probe.hpp"
#include "harness/executor/root_under_test.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::ContextSession;
using av::ExecutorRoot;
using av::RootUnderTest;
using av::MockHostApis;
using av::StateWriter;
using av::Status;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};

}  // namespace

TEST(executor_root_report, names_each_gpu_and_each_live_session)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL, 0u, 0u, 0u);
  apis.add_gpu(nvidia, RVK_EXECUTOR_SUB_API_VULKAN, 0u, 0u, 0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  ContextSession *const on_intel = root.open(RVK_EXECUTOR_SUB_API_VULKAN, intel);
  ContextSession *const on_nvidia = root.open(RVK_EXECUTOR_SUB_API_VULKAN, nvidia);
  CHECK(on_intel != nullptr);
  CHECK(on_nvidia != nullptr);
  root.close(on_intel);
  char bytes[1024];
  StateWriter writer(bytes, sizeof bytes);
  av::StateReporter &reporter = root;
  reporter.report(writer);
  const char *const expected =
      "executor/0 sessions=64 live=1 gpus=2 overflowed=false\n"
      "executor/0/gpu/0 uuid=8680A6460C0000000002000000000000 sub_apis=0x0000000000000005\n"
      "executor/0/gpu/1 uuid=7B7AD2C542198C80C4FDA64B8775AEDB sub_apis=0x0000000000000001\n"
      "executor/0/session/1 api=vulkan gpu=1";
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  CHECK(root.invariants_hold());
  root.close(on_nvidia);
  StateWriter after(bytes, sizeof bytes);
  root.report(after);
  CHECK(std::strstr(after.dump(), "session/") == nullptr);
  CHECK(std::strstr(after.dump(), "live=0") != nullptr); }

TEST(executor_root_report, names_the_api_of_every_kind_of_session)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL | RVK_EXECUTOR_SUB_API_OPENGL,
               0u, 0u, 0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  av::ExecutorRootProbe probe(root);
  av::MockSessionBackend backends[3];
  std::uint8_t           blocks[3][256];
  const std::uint32_t    apis_in_slot[3] = {RVK_EXECUTOR_SUB_API_OPENCL, RVK_EXECUTOR_SUB_API_OPENGL, 16u};
  for (std::uint32_t slot = 0u; slot < 3u; slot++) {
    CHECK_STATUS(probe.session_at(slot).initialize(backends[slot], blocks[slot], sizeof blocks[slot]), Status::ok);
    probe.remember(slot, apis_in_slot[slot], 0u);
  }
  char bytes[1024];
  StateWriter writer(bytes, sizeof bytes);
  root.report(writer);
  CHECK(std::strstr(writer.dump(), "\nexecutor/0/session/0 api=opencl gpu=0\nexecutor/0/session/1 api=opengl gpu=0\n"
                                   "executor/0/session/2 api=unknown gpu=0") != nullptr);
  for (std::uint32_t slot = 0u; slot < 3u; slot++) probe.session_at(slot).release(); }

TEST(executor_root_report, a_root_that_never_asked_has_nothing_but_its_line)
{ RootUnderTest held;
  ExecutorRoot &root = *held;
  char bytes[256];
  StateWriter writer(bytes, sizeof bytes);
  root.report(writer);
  CHECK(std::strcmp(writer.dump(), "executor/0 sessions=64 live=0 gpus=0 overflowed=false") == 0); }
