/* The end of the ABI that answers. What a test asserts here is what the device
 * will act on during realize: the sub-APIs that reach the capset, and the
 * import kinds that decide whether a blob is memory or a copy. */
#include "device/abi/rvk_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/executor_root.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ContextSession;
using av::ExecutorRoot;
using av::MockHostApis;
using av::MockSessionBackend;
using av::Status;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};
constexpr std::uint8_t absent[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

}  // namespace

TEST(executor_root, offers_what_the_host_runs_and_imports_what_each_gpu_can)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL,
               RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF, 0u, RVK_BLOB_DMA_BUF);
  apis.add_gpu(nvidia, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL);

  /* The asymmetry the design exists for, seen through the ABI: the same API
   * imports on one GPU and copies on the other. */
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_OPENCL, intel), std::uint32_t{RVK_BLOB_DMA_BUF});
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_OPENCL, nvidia), 0u);
  CHECK(root.invariants_hold()); }

/* A host with no GPU still loads and still answers; it just offers nothing,
 * which is what stops the device describing a capset it cannot honour. */
TEST(executor_root, a_host_with_no_gpu_offers_nothing)
{ MockHostApis apis;
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, intel), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_gpu_nobody_has_imports_nothing)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, absent), 0u);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, nullptr), 0u); }

/* Before it has asked, it offers nothing rather than guessing. */
TEST(executor_root, answers_nothing_until_it_has_asked)
{ ExecutorRoot root;
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK_EQ(root.import_kinds(RVK_EXECUTOR_SUB_API_VULKAN, intel), 0u);
  CHECK(root.invariants_hold()); }

/* The host is asked once. Asking twice would mean the capset and the answer
 * could disagree. */
TEST(executor_root, asks_the_host_once)
{ MockHostApis apis;
  apis.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_STATUS(root.initialize(apis), Status::invalid_state);
  CHECK_EQ(root.sub_apis(), std::uint32_t{RVK_EXECUTOR_SUB_API_VULKAN}); }

/* More GPUs than the set holds is still a host worth serving, with the ones it
 * kept. */
TEST(executor_root, a_host_with_more_gpus_than_it_holds_still_answers)
{ MockHostApis apis;
  std::uint8_t uuid[16] = {};
  for (std::uint32_t i = 0u; i < MockHostApis::max_gpus; i++) {
      uuid[0] = static_cast<std::uint8_t>(i + 1u);
      apis.add_gpu(uuid, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
  }
  ExecutorRoot root;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), std::uint32_t{RVK_EXECUTOR_SUB_API_VULKAN});
  CHECK(root.invariants_hold()); }


/* The root is sixty four sessions' worth of state, so a test holds one rather
 * than a stack frame's worth of them. */
namespace {

class Opened {
public:
    Opened()
    {
        apis_.add_gpu(intel, RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL,
                      RVK_BLOB_HOST_POINTER, 0u, 0u);
        apis_.add_gpu(nvidia, RVK_EXECUTOR_SUB_API_VULKAN, RVK_BLOB_HOST_POINTER, 0u, 0u);
        status_ = root_.initialize(apis_);
    }
    Status              status() const { return status_; }
    ExecutorRoot       &root() { return root_; }
    MockSessionBackend &backend() { return backend_; }

private:
    MockHostApis       apis_;
    MockSessionBackend backend_;
    ExecutorRoot       root_;
    Status             status_ = Status::ok;
};

}  // namespace

TEST(executor_root_open, a_context_on_a_gpu_that_runs_it_gets_a_session)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  CHECK_EQ(opened.root().live(), 0u);

  ContextSession *const session =
      opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, intel, opened.backend());
  CHECK(session != nullptr);
  CHECK(session->live());
  CHECK_EQ(opened.root().live(), 1u);
  /* The block it decodes into is the library's policy, not a test's. */
  CHECK_EQ(session->starved(), 0u);

  opened.root().close(session);
  CHECK(!session->live());
  CHECK_EQ(opened.root().live(), 0u); }

/* The capset is the union across GPUs, so a guest can ask for an API this GPU
 * does not run. It is refused rather than served by the wrong device. */
TEST(executor_root_open, an_api_this_gpu_does_not_run_gets_no_session)
{ Opened opened;
  CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_OPENCL, nvidia, opened.backend()) == nullptr);
  CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_OPENCL, intel, opened.backend()) != nullptr);
  CHECK_EQ(opened.root().live(), 1u); }

TEST(executor_root_open, a_gpu_nobody_has_gets_no_session)
{ Opened opened;
  CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, absent, opened.backend()) == nullptr);
  CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, nullptr, opened.backend()) == nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

/* Every slot taken is a refusal, not a failure: the device turns it into a
 * context fault and keeps the other contexts running. */
TEST(executor_root_open, every_slot_taken_refuses_the_next)
{ Opened opened;
  for (std::uint32_t i = 0u; i < ExecutorRoot::sessions; i++) {
      CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, intel, opened.backend()) != nullptr);
  }
  CHECK_EQ(opened.root().live(), ExecutorRoot::sessions);
  CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, intel, opened.backend()) == nullptr); }

/* A slot given back is a slot the next context gets. */
TEST(executor_root_open, a_closed_session_s_slot_comes_back)
{ Opened opened;
  ContextSession *first = nullptr;
  for (std::uint32_t i = 0u; i < ExecutorRoot::sessions; i++) {
      ContextSession *const session =
          opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, intel, opened.backend());
      if (i == 0u) first = session;
  }
  opened.root().close(first);
  CHECK_EQ(opened.root().live(), ExecutorRoot::sessions - 1u);
  CHECK(opened.root().open(RVK_EXECUTOR_SUB_API_VULKAN, intel, opened.backend()) == first); }

/* A pointer this root never handed out is not one it will act on, which is
 * what a device that destroys twice would present. */
TEST(executor_root_open, a_session_it_never_handed_out_is_not_closed)
{ Opened opened;
  ContextSession stranger;
  opened.root().close(&stranger);
  opened.root().close(nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

/* Nothing is handed out before the host has been asked. */
TEST(executor_root_open, opens_nothing_until_it_has_asked)
{ MockSessionBackend backend;
  ExecutorRoot root;
  CHECK(root.open(RVK_EXECUTOR_SUB_API_VULKAN, intel, backend) == nullptr); }
