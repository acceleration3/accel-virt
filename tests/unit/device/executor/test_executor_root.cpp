/* The end of the ABI that answers. What a test asserts here is what the device
 * will act on during realize: the sub-APIs that reach the capset, and the
 * import kinds that decide whether a blob is memory or a copy. */
#include "device/abi/av_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/executor_root.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/executor_root_probe.hpp"
#include "harness/executor/root_under_test.hpp"
#include "harness/executor/host_gpu_probe.hpp"
#include "harness/executor/host_gpu_set_probe.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"

#include <cstdint>
#include <unistd.h>

using av::ContextSession;
using av::ExecutorRoot;
using av::RootUnderTest;
using av::ExecutorRootProbe;
using av::HostGpuProbe;
using av::HostGpuSetProbe;
using av::MockHostApis;
using av::MockSessionBackend;
using av::Status;

/* The real-GPU test enumerates the host's own loaders, which allocate things
 * they never give back. Named rather than leak detection being turned off. */
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
           "leak:libz\n";
}

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};
constexpr std::uint8_t absent[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* A resource the table can hold, counted, so a reference given back is seen. */
class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }

    ULONG references = 1u;
};

}  // namespace

TEST(executor_root, offers_what_the_host_runs_and_imports_what_each_gpu_can)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL,
               AV_BLOB_HOST_POINTER | AV_BLOB_DMA_BUF, 0u, AV_BLOB_DMA_BUF);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL);

  /* The asymmetry the design exists for, seen through the ABI: the same API
   * imports on one GPU and copies on the other. */
  CHECK_EQ(root.import_kinds(AV_EXECUTOR_SUB_API_OPENCL, intel), std::uint32_t{AV_BLOB_DMA_BUF});
  CHECK_EQ(root.import_kinds(AV_EXECUTOR_SUB_API_OPENCL, nvidia), 0u);
  CHECK(root.invariants_hold()); }

/* A host with no GPU still loads and still answers; it just offers nothing,
 * which is what stops the device describing a capset it cannot honour. */
TEST(executor_root, a_host_with_no_gpu_offers_nothing)
{ MockHostApis apis;
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK_EQ(root.import_kinds(AV_EXECUTOR_SUB_API_VULKAN, intel), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_gpu_nobody_has_imports_nothing)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.import_kinds(AV_EXECUTOR_SUB_API_VULKAN, absent), 0u);
  CHECK_EQ(root.import_kinds(AV_EXECUTOR_SUB_API_VULKAN, nullptr), 0u); }

/* Before it has asked, it offers nothing rather than guessing. */
TEST(executor_root, answers_nothing_until_it_has_asked)
{ RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK_EQ(root.import_kinds(AV_EXECUTOR_SUB_API_VULKAN, intel), 0u);
  CHECK(root.invariants_hold()); }

/* The guest's release of what it shared reaches the Direct3D 11 table of the
 * GPU it names and no other one; a root that has not asked the host has no
 * table to reach. Id 0 lets go of everything. */
TEST(executor_root, a_shared_release_reaches_only_the_named_gpu_s_table)
{ Counted surface, other;
  MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  apis.give_d3d11(0u);
  CHECK(apis.d3d11().shared().publish(7u, &surface));
  RootUnderTest held;
  ExecutorRoot &root = *held;
  root.release_shared(intel, 7u);
  CHECK_EQ(apis.d3d11().shared().count(), 1u);
  CHECK_STATUS(root.initialize(apis), Status::ok);
  root.release_shared(nvidia, 7u);
  root.release_shared(absent, 7u);
  CHECK_EQ(apis.d3d11().shared().count(), 1u);
  CHECK_EQ(surface.references, 2u);
  root.release_shared(intel, 7u);
  CHECK_EQ(apis.d3d11().shared().count(), 0u);
  CHECK_EQ(surface.references, 1u);
  CHECK(apis.d3d11().shared().publish(8u, &surface));
  CHECK(apis.d3d11().shared().publish(9u, &other));
  root.release_shared(intel, 0u);
  CHECK_EQ(apis.d3d11().shared().count(), 0u);
  CHECK_EQ(surface.references, 1u);
  CHECK_EQ(other.references, 1u);
  CHECK(root.invariants_hold()); }

/* The host is asked once. Asking twice would mean the capset and the answer
 * could disagree. */
TEST(executor_root, asks_the_host_once)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_STATUS(root.initialize(apis), Status::invalid_state);
  CHECK_EQ(root.sub_apis(), std::uint32_t{AV_EXECUTOR_SUB_API_VULKAN}); }

/* More GPUs than the set holds is still a host worth serving, with the ones it
 * kept. */
TEST(executor_root, a_host_with_more_gpus_than_it_holds_still_answers)
{ MockHostApis apis;
  std::uint8_t uuid[16] = {};
  for (std::uint32_t i = 0u; i < MockHostApis::max_gpus; i++) {
      uuid[0] = static_cast<std::uint8_t>(i + 1u);
      apis.add_gpu(uuid, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  }
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), std::uint32_t{AV_EXECUTOR_SUB_API_VULKAN});
  CHECK(root.invariants_hold()); }


/* The root is sixty four sessions' worth of state, so a test holds one rather
 * than a stack frame's worth of them. */
namespace {

class Opened {
public:
    Opened()
    {
        apis_.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL,
                      AV_BLOB_HOST_POINTER, 0u, 0u);
        apis_.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
        status_ = held_->initialize(apis_);
    }
    Status              status() const { return status_; }
    ExecutorRoot       &root() { return *held_; }

private:
    MockHostApis       apis_;
    /* Not by value: the root is eight megabytes and this rig is a local. */
    RootUnderTest      held_;
    Status             status_ = Status::ok;
};

}  // namespace

TEST(executor_root_open, a_context_on_a_gpu_that_runs_it_gets_a_session)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  CHECK_EQ(opened.root().live(), 0u);

  ContextSession *const session =
      opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
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
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_OPENCL, nvidia) == nullptr);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, nvidia) != nullptr);
  CHECK_EQ(opened.root().live(), 1u); }

/* A GPU the capset says runs OpenCL, on a host that kept no OpenCL device for
 * it - which is every mock GPU - runs nothing, so its context is refused at
 * setup and the slot given back, rather than shown an empty platform list. */
TEST(executor_root_open, an_opencl_context_with_no_device_behind_it_gives_the_slot_back)
{ Opened opened;
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_OPENCL, intel) == nullptr);
  CHECK_EQ(opened.root().live(), 0u);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel) != nullptr);
  CHECK_EQ(opened.root().live(), 1u);
  CHECK(opened.root().invariants_hold()); }

TEST(executor_root_open, a_gpu_nobody_has_gets_no_session)
{ Opened opened;
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, absent) == nullptr);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, nullptr) == nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

/* Every slot taken is a refusal, not a failure: the device turns it into a
 * context fault and keeps the other contexts running. */
TEST(executor_root_open, every_slot_taken_refuses_the_next)
{ Opened opened;
  for (std::uint32_t i = 0u; i < ExecutorRoot::sessions; i++) {
      CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel) != nullptr);
  }
  CHECK_EQ(opened.root().live(), ExecutorRoot::sessions);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel) == nullptr); }

/* A slot given back is a slot the next context gets. */
TEST(executor_root_open, a_closed_session_s_slot_comes_back)
{ Opened opened;
  ContextSession *first = nullptr;
  for (std::uint32_t i = 0u; i < ExecutorRoot::sessions; i++) {
      ContextSession *const session =
          opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
      if (i == 0u) first = session;
  }
  opened.root().close(first);
  CHECK_EQ(opened.root().live(), ExecutorRoot::sessions - 1u);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel) == first); }

/* A pointer this root never handed out is not one it will act on, which is
 * what a device that destroys twice would present. */
TEST(executor_root_open, a_session_it_never_handed_out_is_not_closed)
{ Opened opened;
  ContextSession stranger;
  opened.root().close(&stranger);
  opened.root().close(nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

/* The handle an import made, asked through the root the way the ABI asks: a
 * session it handed out answers for its blobs - nothing for an id it never
 * imported, the handle after an import, nothing after the release - and a
 * pointer it never handed out answers nothing without being read. The session
 * is bound to a mock backend in place of its own Vulkan one, which is the part
 * of a real import only a GPU can do. */
TEST(executor_root_open, a_session_s_imported_handle_is_answered_through_the_root)
{ Opened opened;
  ContextSession *const session = &ExecutorRootProbe(opened.root()).session_at(0u);
  static std::uint8_t block[4096];
  MockSessionBackend backend;
  CHECK_STATUS(session->initialize(backend, block, sizeof block), Status::ok);
  static std::uint8_t page[4096];
  AvBlobHandle handle = {};
  handle.kind = AV_BLOB_HOST_POINTER;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;

  CHECK_EQ(opened.root().imported_handle(session, 9u), 0u);
  CHECK_EQ(session->import_blob(9u, handle), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(opened.root().imported_handle(session, 9u), 9u + MockSessionBackend::wire_offset);
  CHECK_EQ(opened.root().imported_handle(session, 10u), 0u);

  static std::uint8_t stranger_block[4096];
  MockSessionBackend stranger_backend;
  ContextSession stranger;
  CHECK_STATUS(stranger.initialize(stranger_backend, stranger_block, sizeof stranger_block), Status::ok);
  CHECK_EQ(stranger.import_blob(9u, handle), std::uint32_t{AV_EXECUTED});
  CHECK(stranger.imported_handle(9u) != 0u);
  CHECK_EQ(opened.root().imported_handle(&stranger, 9u), 0u);
  CHECK_EQ(opened.root().imported_handle(nullptr, 9u), 0u);
  stranger.release();

  session->release_blob(9u);
  CHECK_EQ(opened.root().imported_handle(session, 9u), 0u);
  session->release(); }

/* Nothing is handed out before the host has been asked. */
TEST(executor_root_open, opens_nothing_until_it_has_asked)
{ RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK(root.open(AV_EXECUTOR_SUB_API_VULKAN, intel) == nullptr); }

/* The root answers the capset from its GPUs, so one broken GPU is a broken
 * root. */
TEST(executor_root_invariants, a_gpu_that_breaks_its_own_breaks_them)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  HostGpuSetProbe set(ExecutorRootProbe(opened.root()).gpus());
  HostGpuProbe(set.gpu_at(0u)).set_sub_apis(AV_EXECUTOR_SUB_API_OPENCL);
  CHECK(!opened.root().invariants_hold()); }

/* GPUs it has not asked about are GPUs sub_apis() would hide and open() would
 * refuse, while the set says they are there. */
TEST(executor_root_invariants, gpus_it_never_asked_about_break_them)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  ExecutorRootProbe(opened.root()).forget_host();
  CHECK(!opened.root().invariants_hold()); }

/* Two sub-API bits at once pass a GPU that runs both and still name no single
 * backend, so no session is made rather than one bound to the wrong API. */
TEST(executor_root_open, a_value_naming_two_sub_apis_at_once_gets_no_session)
{ Opened opened;
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL, intel) ==
        nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

/* A GPU the host says runs OpenGL can still refuse a context - this mock GPU
 * has no EGL device behind it at all - and then the slot it would have used is
 * given back rather than left bound to a backend that cannot run anything.
 * `absent` rather than `intel`: the Intel UUID in this file is a real one, and
 * on the machine it came from OpenGL opens. */
TEST(executor_root_open, an_opengl_context_that_will_not_open_gives_the_slot_back)
{ MockHostApis apis;
  apis.add_gpu(absent, AV_EXECUTOR_SUB_API_OPENGL, 0u, 0u, 0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK(root.open(AV_EXECUTOR_SUB_API_OPENGL, absent) == nullptr);
  CHECK_EQ(root.live(), 0u);
  CHECK(root.invariants_hold()); }

/* And contexts of every API on the GPUs this host really has, found from its
 * own enumeration: each API a GPU runs opens there, on what the host kept for
 * it, and closing one leaves the others running - two OpenGL contexts on one
 * GPU share its display, and closing the first used to terminate it under the
 * second. A host with no GPU has nothing here to judge. */
TEST(executor_root_open, contexts_on_a_real_gpu_are_sessions_that_close_alone)
{ av::LoaderHostApis host;
  host.open();
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(host), Status::ok);
  constexpr std::uint32_t apis[] = {AV_EXECUTOR_SUB_API_VULKAN, AV_EXECUTOR_SUB_API_OPENCL,
                                    AV_EXECUTOR_SUB_API_OPENGL};
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      std::uint8_t uuid[16] = {};
      host.gpu_uuid(i, uuid);
      for (const std::uint32_t api : apis) {
          ContextSession *const first = root.open(api, uuid);
          ContextSession *const second = root.open(api, uuid);
          const bool runs = (host.sub_apis(i) & api) != 0u;
          CHECK_EQ(first != nullptr, runs);
          CHECK_EQ(second != nullptr, runs);
          if (!runs) continue;
          root.close(first);
          CHECK_EQ(root.live(), 1u);
          CHECK(second->live());
          if (api == AV_EXECUTOR_SUB_API_OPENGL) {
              /* glFinish, which a context whose display was terminated
               * cannot run. */
              std::uint8_t reply[8] = {};
              std::size_t  answer = 0u;
              CHECK_EQ(second->execute(av::glproto::kCmd_glFinish, nullptr, 0u, reply, sizeof reply, answer),
                       std::uint32_t{AV_EXECUTED});
          }
          root.close(second);
          CHECK_EQ(root.live(), 0u);
      }
  }
  host.close(); }

/* Version 4 (feature 050): only OpenGL takes options or a share, whatever
 * field of the options is set, and a share that is not a session of this root
 * - a stranger, a closed one, another API's - makes nothing. */
TEST(executor_root_open, options_or_a_share_for_another_api_get_no_session)
{ Opened opened;
  const AvContextOptions asked[] = {{1u, 0u, 0u, 0u, 0u}, {0u, 1u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u},
                                     {0u, 0u, 0u, 1u, 0u}, {0u, 0u, 0u, 0u, 1u}};
  for (const AvContextOptions &options : asked)
      CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel, &options, nullptr) == nullptr);
  const AvContextOptions defaults = {};
  ContextSession *const vulkan = opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel, &defaults, nullptr);
  CHECK(vulkan != nullptr);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_VULKAN, intel, nullptr, vulkan) == nullptr);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_OPENGL, intel, nullptr, vulkan) == nullptr);
  ContextSession stranger;
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_OPENGL, intel, nullptr, &stranger) == nullptr);
  opened.root().close(vulkan);
  CHECK(opened.root().open(AV_EXECUTOR_SUB_API_OPENGL, intel, nullptr, vulkan) == nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

/* On the GPUs this host has: an OpenGL session shares another on its own GPU
 * and not one on another GPU, and one made with options it cannot have is
 * refused with its slot given back. */
TEST(executor_root_open, an_opengl_session_shares_one_on_its_own_gpu)
{ av::LoaderHostApis host;
  host.open();
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(host), Status::ok);
  ContextSession *previous = nullptr;
  const AvContextOptions core = {4u, 3u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
  const AvContextOptions impossible = {4u, 7u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
  for (std::uint32_t i = 0u; i < host.gpu_count(); i++) {
      if ((host.sub_apis(i) & AV_EXECUTOR_SUB_API_OPENGL) == 0u) continue;
      std::uint8_t uuid[16] = {};
      host.gpu_uuid(i, uuid);
      ContextSession *const first = root.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, nullptr, nullptr);
      CHECK(first != nullptr);
      ContextSession *const second = root.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, &core, first);
      CHECK(second != nullptr);
      const std::uint32_t live = root.live();
      CHECK(root.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, &impossible, nullptr) == nullptr);
      CHECK_EQ(root.live(), live);
      if (previous != nullptr) CHECK(root.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, &core, previous) == nullptr);
      root.close(second);
      if (previous != nullptr) root.close(previous);
      previous = first;
  }
  if (previous != nullptr) root.close(previous);
  CHECK_EQ(root.live(), 0u);
  host.close(); }

/* A session this root never handed out is one it binds nothing to.
 *
 * The signal is routed by finding the session in the root's own array, so a
 * stranger's - another root's, or one on the stack - must fall off the end
 * rather than be matched to whatever sits at the same index. */
TEST(executor_root_open, a_signal_for_a_session_this_root_never_made_binds_nothing)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  ContextSession stranger;
  /* It simply returns: there is nothing to bind and nothing to report, and
   * the root is untouched. */
  opened.root().set_signal(&stranger, nullptr, nullptr);
  CHECK_EQ(opened.root().live(), 0u);
  CHECK(opened.root().invariants_hold()); }

/* Feature 053: the three shared-object calls reach only a session this root
 * handed out. The device's pointer is all the ABI gives to tell them apart, so
 * a stranger's session and a null are the two a root must answer nothing to. */
TEST(executor_root, the_shared_object_calls_reach_only_a_session_this_root_opened)
{ Opened opened;
  ContextSession *const session = &ExecutorRootProbe(opened.root()).session_at(0u);
  static std::uint8_t   block[4096];
  MockSessionBackend    backend;
  CHECK_STATUS(session->initialize(backend, block, sizeof block), Status::ok);

  /* A session this root owns whose backend shares nothing is a refusal the
   * root passes on rather than one it makes itself. */
  backend.set_shares(false);
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!opened.root().export_object(session, AV_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(fd, -1);
  std::uint64_t refused = 9u;
  CHECK(!opened.root().import_object(session, AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 3, 4096u,
                                     refused));
  CHECK_EQ(refused, 0u);
  backend.set_shares(true);

  CHECK(opened.root().export_object(session, AV_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK(fd >= 0);
  std::uint64_t made = 0u;
  CHECK(opened.root().import_object(session, AV_SHARED_MEMORY, handle_kind, fd, 4096u, made));
  CHECK(made != 0u);
  opened.root().release_object(session, AV_SHARED_MEMORY, made);
  CHECK_EQ(backend.released_objects(), 1u);

  static std::uint8_t stranger_block[4096];
  MockSessionBackend  stranger_backend;
  ContextSession      stranger;
  CHECK_STATUS(stranger.initialize(stranger_backend, stranger_block, sizeof stranger_block), Status::ok);
  std::uint32_t theirs_kind = 9u;
  int           theirs_fd = 9;
  CHECK(!opened.root().export_object(&stranger, AV_SHARED_MEMORY, 0x40u, theirs_kind, theirs_fd));
  CHECK_EQ(theirs_fd, -1);
  CHECK(!opened.root().export_object(nullptr, AV_SHARED_MEMORY, 0x40u, theirs_kind, theirs_fd));
  std::uint64_t theirs = 9u;
  CHECK(!opened.root().import_object(&stranger, AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, fd, 4096u,
                                     theirs));
  CHECK_EQ(theirs, 0u);
  CHECK(!opened.root().import_object(nullptr, AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, fd, 4096u,
                                     theirs));
  opened.root().release_object(&stranger, AV_SHARED_MEMORY, made);
  opened.root().release_object(nullptr, AV_SHARED_MEMORY, made);
  CHECK_EQ(stranger_backend.released_objects(), 0u);
  CHECK_EQ(backend.released_objects(), 1u);
  stranger.release();
  ::close(fd);
  session->release(); }
