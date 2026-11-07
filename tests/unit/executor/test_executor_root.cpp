#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "executor/executor_log.hpp"
#include "executor/executor_root.hpp"
#include "executor/sharing/d3d_shares.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/watcher_exit.hpp"
#include "proto/resources/surface_point.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/generous_budget.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "harness/executor/executor_root_probe.hpp"
#include "harness/executor/root_under_test.hpp"
#include "harness/executor/host_gpu_probe.hpp"
#include "harness/executor/host_gpu_set_probe.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/executor/software_gl.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"

#include <atomic>
#include <cstdint>
#include <pthread.h>
#include <cstdio>
#include <time.h>
#include <unistd.h>

using av::ContextSession;
using av::ExecutorRoot;
using av::RootUnderTest;
using av::ExecutorRootProbe;
using av::HostGpuProbe;
using av::HostGpuSetProbe;
using av::MockHostApis;
using av::MockSessionBackend;
using av::HostShares;
using av::ShareOrigin;
using av::Status;

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

class QuietLog {
public:
    QuietLog() : sink_(), lines_(sink_, "t: ") {}
    av::DriverLog &lines() { return lines_; }

private:
    av::RecordingLog sink_;
    av::DriverLog    lines_;
};

}

TEST(executor_root, offers_what_the_host_runs)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_host_with_no_gpu_offers_nothing)
{ MockHostApis apis;
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, answers_nothing_until_it_has_asked)
{ RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_EQ(root.sub_apis(), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root_memory, a_domain_allocates_from_its_gpu_s_source_and_frees_by_key)
{ MockHostApis apis;
  apis.exporter().add_type(av::MockMemoryExporter::host_visible | av::MockMemoryExporter::host_coherent |
                               av::MockMemoryExporter::host_cached,
                           true);
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  const std::uint32_t domain = held.domain_of(intel);
  const std::uint32_t readback = av::HostMemoryRequest(av::HostMemoryRequest::Kind::readback, 0u).encoded();
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(root.allocate_memory(domain + 100u, 7u, 4096u, readback, memory), Status::no_memory);
  CHECK_EQ(memory.fd, -1);
  CHECK_STATUS(root.allocate_memory(domain, 7u, 4096u, readback, memory), Status::ok);
  CHECK(memory.fd >= 0);
  CHECK_EQ(memory.size, 4096u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  CHECK_EQ(apis.source().live(), 1u);
  root.free_memory(domain + 100u, 7u);
  CHECK_EQ(apis.source().live(), 1u);
  root.free_memory(domain, 7u);
  CHECK_EQ(apis.source().live(), 0u);
  CHECK_EQ(apis.exporter().live(), 0u);
  close(memory.fd);
  CHECK(root.invariants_hold()); }

TEST(executor_root_memory, closing_a_domain_frees_what_it_still_held)
{ MockHostApis apis;
  apis.exporter().add_type(av::MockMemoryExporter::host_visible | av::MockMemoryExporter::host_coherent, true);
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  const std::uint32_t domain = held.domain_of(intel);
  const std::uint32_t upload = av::HostMemoryRequest(av::HostMemoryRequest::Kind::upload, 0u).encoded();
  AvHostMemory first = {-1, 0u, 0u};
  AvHostMemory second = {-1, 0u, 0u};
  CHECK_STATUS(root.allocate_memory(domain, 1u, 4096u, upload, first), Status::ok);
  CHECK_STATUS(root.allocate_memory(domain, 2u, 8192u, upload, second), Status::ok);
  CHECK_EQ(first.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  CHECK_EQ(apis.exporter().live(), 2u);
  root.close_domain(domain);
  CHECK_EQ(apis.exporter().live(), 0u);
  close(first.fd);
  close(second.fd);
  CHECK(root.invariants_hold()); }

TEST(executor_root_memory, a_gpu_with_no_memory_source_gives_none)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  apis.withhold_memory_source();
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  const std::uint32_t domain = held.domain_of(intel);
  CHECK(domain != 0u);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(root.allocate_memory(domain, 1u, 4096u, 1u, memory), Status::no_memory);
  CHECK_EQ(memory.fd, -1);
  root.free_memory(domain, 1u);
  root.close_domain(domain);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_domain_is_opened_on_a_gpu_the_host_has_and_closed_once)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_EQ(root.open_domain(intel), 0u);
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.open_domain(absent), 0u);
  CHECK_EQ(root.open_domain(nullptr), 0u);
  const std::uint32_t domain = root.open_domain(intel);
  CHECK(domain != 0u);
  CHECK(root.open_domain(intel) != domain);
  const AvContextOptions in_domain = {0u, 0u, 0u, 0u, domain};
  ContextSession *const session = root.open(AV_EXECUTOR_SUB_API_VULKAN, in_domain);
  CHECK(session != nullptr);
  root.close(session);
  root.close_domain(domain);
  CHECK(root.open(AV_EXECUTOR_SUB_API_VULKAN, in_domain) == nullptr);
  const AvContextOptions none = {};
  CHECK(root.open(AV_EXECUTOR_SUB_API_VULKAN, none) == nullptr);
  root.close_domain(domain);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_shared_release_reaches_only_the_named_domain)
{ Counted surface, other, elsewhere;
  MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  root.release_shared(1u, 7u);
  CHECK_STATUS(root.initialize(apis), Status::ok);
  const std::uint32_t on_intel = held.domain_of(intel);
  const std::uint32_t on_nvidia = held.domain_of(nvidia);
  HostShares        &shares = *apis.shares(0u);
  av::D3dShares      d3d(shares);
  MockSessionBackend publisher;
  CHECK_STATUS(d3d.publish_resource(on_intel, 7u, &surface, ShareOrigin::d3d11, publisher), Status::ok);
  root.release_shared(on_nvidia, 7u);
  root.release_shared(on_intel + 100u, 7u);
  CHECK_EQ(shares.live(), 1u);
  root.release_shared(on_intel, 7u);
  CHECK_EQ(shares.live(), 0u);
  CHECK_STATUS(d3d.publish_resource(on_intel, 8u, &surface, ShareOrigin::d3d11, publisher), Status::ok);
  CHECK_STATUS(d3d.publish_resource(on_intel, 9u, &other, ShareOrigin::d3d11, publisher), Status::ok);
  CHECK_STATUS(d3d.publish_resource(on_nvidia, 8u, &elsewhere, ShareOrigin::d3d11, publisher), Status::ok);
  root.release_shared(on_intel, 0u);
  CHECK_EQ(shares.live(), 1u);
  root.close_domain(on_nvidia);
  CHECK_EQ(shares.live(), 0u);
  IUnknown *objects[4] = {};
  for (std::uint32_t taken = d3d.take_retired(objects, 4u); taken != 0u; taken = d3d.take_retired(objects, 4u))
      for (std::uint32_t index = 0u; index < taken; index++) objects[index]->Release();
  CHECK_EQ(surface.references, 1u);
  CHECK_EQ(other.references, 1u);
  CHECK_EQ(elsewhere.references, 1u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_surface_is_exported_only_through_the_domain_s_device)
{ Counted surface;
  MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_D3D);
  apis.add_gpu(nvidia, AV_EXECUTOR_SUB_API_D3D);
  apis.give_d3d11(0u);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  AvExportedImage image = {};
  std::uint64_t   writes = 5u;
  int             fd = 5;
  CHECK(!root.export_surface(1u, 7u, 0u, image));
  CHECK(!root.sync_surface(1u, 7u, 0u, writes, fd));
  CHECK_STATUS(root.initialize(apis), Status::ok);
  const std::uint32_t on_intel = held.domain_of(intel);
  const std::uint32_t on_nvidia = held.domain_of(nvidia);
  MockSessionBackend  publisher;
  CHECK_STATUS(av::D3dShares(*apis.shares(0u)).publish_resource(on_intel, 7u, &surface, ShareOrigin::d3d11, publisher),
               Status::ok);
  CHECK(!root.export_surface(on_intel + 100u, 7u, 0u, image));
  CHECK(!root.export_surface(on_nvidia, 7u, 0u, image));
  CHECK(!root.export_surface(on_intel, 7u, 0u, image));
  CHECK_EQ(image.fd, -1);
  CHECK(!root.export_image(on_intel, 7u, image));
  CHECK(!root.export_image(on_nvidia, 7u, image));
  CHECK(!root.export_image(on_intel + 100u, 7u, image));
  std::uint8_t pixels[16] = {};
  CHECK(!root.read_image(on_intel, 7u, pixels, 8u, 2u, 2u));
  CHECK(!root.read_image(on_nvidia, 7u, pixels, 8u, 2u, 2u));
  CHECK(!root.read_image(on_intel + 100u, 7u, pixels, 8u, 2u, 2u));
  CHECK(!root.sync_surface(on_intel + 100u, 7u, 0u, writes, fd));
  CHECK(!root.sync_surface(on_nvidia, 7u, 0u, writes, fd));
  CHECK(!root.sync_surface(on_intel, 7u, 0u, writes, fd));
  CHECK_EQ(fd, -1);
  CHECK_EQ(writes, 0u);
  apis.shares(0u)->clear();
  IUnknown *objects[1] = {};
  while (av::D3dShares(*apis.shares(0u)).take_retired(objects, 1u) != 0u) objects[0]->Release();
  CHECK_EQ(surface.references, 1u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, frames_are_read_only_in_an_open_domain)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_D3D);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  const std::uint32_t on_intel = held.domain_of(intel);
  AvFrameView view = {};
  CHECK(!root.map_frame(on_intel + 100u, av::SurfacePoint(3u, 1u), 0u, view));
  CHECK(!root.map_frame(on_intel, av::SurfacePoint(3u, 1u), 0u, view));
  root.unmap_frame(on_intel + 100u, av::SurfacePoint(3u, 1u));
  root.unmap_frame(on_intel, av::SurfacePoint(3u, 1u));
  CHECK(root.invariants_hold()); }

TEST(executor_root, asks_the_host_once)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_STATUS(root.initialize(apis), Status::invalid_state);
  CHECK_EQ(root.sub_apis(), std::uint32_t{AV_EXECUTOR_SUB_API_VULKAN}); }

TEST(executor_root_memory, a_session_on_a_gpu_with_no_source_opens_and_imports_nothing)
{ MockHostApis apis;
  apis.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  apis.withhold_memory_source();
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  ContextSession *const session = held.open(AV_EXECUTOR_SUB_API_VULKAN, intel);
  CHECK(session != nullptr);
  if (session == nullptr) return;
  CHECK_EQ(session->import_blob(1u, 1u), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(session->imports().count(), 0u);
  root.close(session);
  CHECK(root.invariants_hold()); }

TEST(executor_root, a_host_with_more_gpus_than_it_holds_still_answers)
{ MockHostApis apis;
  std::uint8_t uuid[16] = {};
  for (std::uint32_t i = 0u; i < MockHostApis::max_gpus; i++) {
      uuid[0] = static_cast<std::uint8_t>(i + 1u);
      apis.add_gpu(uuid, AV_EXECUTOR_SUB_API_VULKAN);
  }
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK_EQ(root.sub_apis(), std::uint32_t{AV_EXECUTOR_SUB_API_VULKAN});
  CHECK(root.invariants_hold()); }


namespace {

class Opened {
public:
    Opened()
    {
        apis_.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL);
        apis_.add_gpu(nvidia, AV_EXECUTOR_SUB_API_VULKAN);
        status_ = held_->initialize(apis_);
    }
    Status              status() const { return status_; }
    ExecutorRoot       &root() { return *held_; }
    RootUnderTest      &held() { return held_; }
    MockHostApis       &apis() { return apis_; }

private:
    MockHostApis       apis_;
    RootUnderTest      held_;
    Status             status_ = Status::ok;
};

}

TEST(executor_root_open, a_context_on_a_gpu_that_runs_it_gets_a_session)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  CHECK_EQ(opened.root().live(), 0u);

  ContextSession *const session =
      opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
  CHECK(session != nullptr);
  CHECK(session->live());
  CHECK_EQ(opened.root().live(), 1u);
  CHECK_EQ(session->starved(), 0u);

  opened.root().close(session);
  CHECK(!session->live());
  CHECK_EQ(opened.root().live(), 0u); }

TEST(executor_root_open, an_api_this_gpu_does_not_run_gets_no_session)
{ Opened opened;
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_OPENCL, nvidia) == nullptr);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, nvidia) != nullptr);
  CHECK_EQ(opened.root().live(), 1u); }

TEST(executor_root_open, an_opencl_context_with_no_device_behind_it_gives_the_slot_back)
{ Opened opened;
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_OPENCL, intel) == nullptr);
  CHECK_EQ(opened.root().live(), 0u);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel) != nullptr);
  CHECK_EQ(opened.root().live(), 1u);
  CHECK(opened.root().invariants_hold()); }

TEST(executor_root_open, a_gpu_nobody_has_gets_no_session)
{ Opened opened;
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, absent) == nullptr);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, nullptr) == nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

TEST(executor_root_open, every_slot_taken_refuses_the_next)
{ Opened opened;
  for (std::uint32_t i = 0u; i < ExecutorRoot::sessions; i++) {
      CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel) != nullptr);
  }
  CHECK_EQ(opened.root().live(), ExecutorRoot::sessions);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel) == nullptr); }

TEST(executor_root_open, a_closed_session_s_slot_comes_back)
{ Opened opened;
  ContextSession *first = nullptr;
  for (std::uint32_t i = 0u; i < ExecutorRoot::sessions; i++) {
      ContextSession *const session =
          opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
      if (i == 0u) first = session;
  }
  opened.root().close(first);
  CHECK_EQ(opened.root().live(), ExecutorRoot::sessions - 1u);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel) == first); }

TEST(executor_root_open, a_session_it_never_handed_out_is_not_closed)
{ Opened opened;
  QuietLog        quiet;
  ContextSession  stranger(quiet.lines());
  opened.root().close(&stranger);
  opened.root().close(nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

TEST(executor_root_open, a_session_s_imported_handle_is_answered_through_the_root)
{ Opened opened;
  ContextSession *const session = &ExecutorRootProbe(opened.root()).session_at(0u);
  static std::uint8_t block[4096];
  MockSessionBackend backend;
  CHECK_STATUS(session->initialize(backend, block, sizeof block), Status::ok);
  opened.apis().exporter().add_type(av::MockMemoryExporter::host_visible | av::MockMemoryExporter::host_coherent,
                                    true);
  const std::uint32_t domain = opened.held().domain_of(intel);
  AvHostMemory        memory = {-1, 0u, 0u};
  CHECK_STATUS(opened.root().allocate_memory(domain, 0x30u, 4096u, 1u, memory), Status::ok);
  session->draw_from(opened.apis().source(), domain);

  CHECK_EQ(opened.root().imported_handle(session, 9u), 0u);
  CHECK_EQ(session->import_blob(9u, 0x30u), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(opened.root().imported_handle(session, 9u), 1u + MockSessionBackend::wire_offset);
  CHECK_EQ(opened.root().imported_handle(session, 10u), 0u);

  static std::uint8_t stranger_block[4096];
  MockSessionBackend stranger_backend;
  QuietLog        quiet;
  ContextSession  stranger(quiet.lines());
  CHECK_STATUS(stranger.initialize(stranger_backend, stranger_block, sizeof stranger_block), Status::ok);
  stranger.draw_from(opened.apis().source(), domain);
  CHECK_EQ(stranger.import_blob(9u, 0x30u), std::uint32_t{AV_EXECUTED});
  CHECK(stranger.imported_handle(9u) != 0u);
  CHECK_EQ(opened.root().imported_handle(&stranger, 9u), 0u);
  CHECK_EQ(opened.root().imported_handle(nullptr, 9u), 0u);
  static_cast<void>(stranger.release(av::GenerousBudget().budget())); stranger.vacate();

  session->release_blob(9u);
  CHECK_EQ(opened.root().imported_handle(session, 9u), 0u);
  static_cast<void>(session->release(av::GenerousBudget().budget())); session->vacate();
  opened.root().free_memory(domain, 0x30u);
  CHECK_EQ(opened.apis().exporter().live(), 0u);
  close(memory.fd); }

TEST(executor_root_open, opens_nothing_until_it_has_asked)
{ RootUnderTest held;
  CHECK_EQ(held.domain_of(intel), 0u);
  CHECK(held.open(AV_EXECUTOR_SUB_API_VULKAN, intel) == nullptr); }

TEST(executor_root_invariants, a_gpu_that_breaks_its_own_breaks_them)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  HostGpuSetProbe set(ExecutorRootProbe(opened.root()).gpus());
  set.set_count(3u);
  HostGpuProbe(set.gpu_at(2u)).set_sub_apis(AV_EXECUTOR_SUB_API_OPENCL);
  CHECK(!opened.root().invariants_hold());
  HostGpuProbe(set.gpu_at(2u)).set_sub_apis(0u);
  set.set_count(2u); }

TEST(executor_root_invariants, gpus_it_never_asked_about_break_them)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  ExecutorRootProbe(opened.root()).forget_host();
  CHECK(!opened.root().invariants_hold()); }

TEST(executor_root_open, a_value_naming_two_sub_apis_at_once_gets_no_session)
{ Opened opened;
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENCL, intel) ==
        nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

TEST(executor_root_open, an_opengl_context_that_will_not_open_gives_the_slot_back)
{ MockHostApis apis;
  apis.add_gpu(absent, AV_EXECUTOR_SUB_API_OPENGL);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  CHECK(held.open(AV_EXECUTOR_SUB_API_OPENGL, absent) == nullptr);
  CHECK_EQ(root.live(), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root_open, contexts_on_a_real_gpu_are_sessions_that_close_alone)
{ av::RecordingLog   sink;
  av::ExecutorLog    log(sink);
  av::LoaderHostApis host(log);
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
          ContextSession *const first = held.open(api, uuid);
          ContextSession *const second = held.open(api, uuid);
          const bool runs = (host.sub_apis(i) & api) != 0u;
          CHECK_EQ(first != nullptr, runs);
          CHECK_EQ(second != nullptr, runs);
          if (!runs) continue;
          root.close(first);
          CHECK_EQ(root.live(), 1u);
          CHECK(second->live());
          if (api == AV_EXECUTOR_SUB_API_OPENGL) {
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

TEST(executor_root_open, options_or_a_share_for_another_api_get_no_session)
{ Opened opened;
  const AvContextOptions asked[] = {{1u, 0u, 0u, 0u, 0u}, {0u, 1u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u},
                                     {0u, 0u, 0u, 1u, 0u}};
  for (const AvContextOptions &options : asked)
      CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel, &options, nullptr) == nullptr);
  const AvContextOptions defaults = {};
  ContextSession *const vulkan = opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel, &defaults, nullptr);
  CHECK(vulkan != nullptr);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel, nullptr, vulkan) == nullptr);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_OPENGL, intel, nullptr, vulkan) == nullptr);
  QuietLog        quiet;
  ContextSession  stranger(quiet.lines());
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_OPENGL, intel, nullptr, &stranger) == nullptr);
  opened.root().close(vulkan);
  CHECK(opened.held().open(AV_EXECUTOR_SUB_API_OPENGL, intel, nullptr, vulkan) == nullptr);
  CHECK_EQ(opened.root().live(), 0u); }

TEST(executor_root_open, an_opengl_session_shares_one_on_its_own_gpu)
{ av::RecordingLog   sink;
  av::ExecutorLog    log(sink);
  av::LoaderHostApis host(log);
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
      ContextSession *const first = held.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, nullptr, nullptr);
      CHECK(first != nullptr);
      ContextSession *const second = held.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, &core, first);
      CHECK(second != nullptr);
      const std::uint32_t live = root.live();
      CHECK(held.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, &impossible, nullptr) == nullptr);
      CHECK_EQ(root.live(), live);
      if (previous != nullptr) CHECK(held.open(AV_EXECUTOR_SUB_API_OPENGL, uuid, &core, previous) == nullptr);
      root.close(second);
      if (previous != nullptr) root.close(previous);
      previous = first;
  }
  if (previous != nullptr) root.close(previous);
  CHECK_EQ(root.live(), 0u);
  host.close(); }

TEST(executor_root_open, a_signal_for_a_session_this_root_never_made_binds_nothing)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  QuietLog        quiet;
  ContextSession  stranger(quiet.lines());
  opened.root().set_signal(&stranger, nullptr, nullptr);
  CHECK_EQ(opened.root().live(), 0u);
  CHECK(opened.root().invariants_hold()); }

TEST(executor_root_lose_device, only_a_session_with_a_host_device_this_root_made_loses_one)
{ Opened opened;
  CHECK_STATUS(opened.status(), Status::ok);
  QuietLog        quiet;
  ContextSession  stranger(quiet.lines());
  CHECK_EQ(opened.root().lose_device(nullptr), std::uint32_t{AV_UNKNOWN});
  CHECK_EQ(opened.root().lose_device(&stranger), std::uint32_t{AV_UNKNOWN});
  ContextSession *const session = opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
  CHECK(session != nullptr);
  CHECK_EQ(opened.root().lose_device(session), std::uint32_t{AV_UNKNOWN});
  CHECK_EQ(session->poll(), std::uint32_t{AV_POLL_IDLE});
  opened.root().close(session);
  CHECK(opened.root().invariants_hold()); }

TEST(executor_root, the_shared_object_calls_reach_the_domain_as_the_session_that_asked)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t maker_block[4096], reader_block[4096];
  MockSessionBackend  maker_backend, reader_backend;
  ContextSession *const maker = &probe.session_at(0u);
  ContextSession *const reader = &probe.session_at(1u);
  CHECK_STATUS(maker->initialize(maker_backend, maker_block, sizeof maker_block), Status::ok);
  CHECK_STATUS(reader->initialize(reader_backend, reader_block, sizeof reader_block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, maker_backend);
  probe.shares_at(1u).bind(*opened.apis().shares(0u), domain, reader_backend);

  CHECK_EQ(root.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  CHECK_EQ(root.share_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x41u), std::uint32_t{AV_SHARED_CONFLICT});
  std::uint64_t made = 9u;
  CHECK_EQ(root.open_object(reader, 0x2Bu, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_UNKNOWN});
  CHECK_EQ(made, 0u);
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 8192u, 0u, made), std::uint32_t{AV_SHARED_MISMATCHED});
  reader_backend.set_shares(false);
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_REFUSED});
  reader_backend.set_shares(true);
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_DONE});
  CHECK(made != 0u);
  root.close_object(reader, 0x2Au, made);
  CHECK_EQ(reader_backend.released_objects(), 1u);

  static std::uint8_t stranger_block[4096];
  MockSessionBackend  stranger_backend;
  QuietLog        quiet;
  ContextSession  stranger(quiet.lines());
  CHECK_STATUS(stranger.initialize(stranger_backend, stranger_block, sizeof stranger_block), Status::ok);
  CHECK_EQ(root.share_object(&stranger, 0x2Cu, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_REFUSED});
  CHECK_EQ(root.share_object(nullptr, 0x2Cu, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_REFUSED});
  CHECK_EQ(root.open_object(&stranger, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_REFUSED});
  CHECK_EQ(root.open_object(nullptr, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_REFUSED});
  root.close_object(&stranger, 0x2Au, made);
  root.close_object(nullptr, 0x2Au, made);
  CHECK_EQ(stranger_backend.released_objects(), 0u);

  std::size_t answer = 0u;
  CHECK_EQ(reader->execute(MockSessionBackend::lose_id, nullptr, 0u, nullptr, 0u, answer),
           std::uint32_t{AV_DEVICE_LOST});
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_REFUSED});
  CHECK_EQ(root.share_object(reader, 0x2Du, AV_SHARED_MEMORY, 4096u, 0x41u), std::uint32_t{AV_SHARED_REFUSED});
  static_cast<void>(stranger.release(av::GenerousBudget().budget())); stranger.vacate();
  probe.shares_at(1u).end();
  probe.shares_at(0u).end();
  static_cast<void>(reader->release(av::GenerousBudget().budget())); reader->vacate();
  static_cast<void>(maker->release(av::GenerousBudget().budget())); maker->vacate(); }

TEST(executor_root, closing_a_session_shuts_down_the_backend_it_ran_after_its_shares)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t maker_block[4096], reader_block[4096];
  MockSessionBackend  maker_backend, reader_backend;
  ContextSession *const maker = &probe.session_at(0u);
  ContextSession *const reader = &probe.session_at(1u);
  CHECK_STATUS(maker->initialize(maker_backend, maker_block, sizeof maker_block), Status::ok);
  CHECK_STATUS(reader->initialize(reader_backend, reader_block, sizeof reader_block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, maker_backend);
  probe.shares_at(1u).bind(*opened.apis().shares(0u), domain, reader_backend);
  probe.remember(0u, AV_EXECUTOR_SUB_API_VULKAN, 0u);
  probe.remember(1u, AV_EXECUTOR_SUB_API_OPENCL, 0u);
  CHECK_EQ(root.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  std::uint64_t made = 0u;
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_DONE});

  root.close(reader);
  CHECK_EQ(reader_backend.shut_downs(), 1u);
  CHECK_EQ(reader_backend.released_before_shut_down(), 1u);
  CHECK(!reader->live());
  CHECK_EQ(maker_backend.shut_downs(), 0u);
  CHECK(maker->live());
  CHECK(root.invariants_hold());

  root.close(reader);
  CHECK_EQ(reader_backend.shut_downs(), 1u);
  root.close(maker);
  CHECK_EQ(maker_backend.shut_downs(), 1u);
  CHECK(root.invariants_hold()); }

TEST(executor_root, an_object_is_opened_into_what_the_session_names_and_closed_by_that_handle)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t maker_block[4096], reader_block[4096];
  MockSessionBackend  maker_backend, reader_backend;
  ContextSession *const maker = &probe.session_at(0u);
  ContextSession *const reader = &probe.session_at(1u);
  CHECK_STATUS(maker->initialize(maker_backend, maker_block, sizeof maker_block), Status::ok);
  CHECK_STATUS(reader->initialize(reader_backend, reader_block, sizeof reader_block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, maker_backend);
  probe.shares_at(1u).bind(*opened.apis().shares(0u), domain, reader_backend);
  CHECK_EQ(root.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  std::uint64_t device = 0u;
  std::uint64_t semaphore = 0u;
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x7001u, device), std::uint32_t{AV_SHARED_DONE});
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x7002u, semaphore),
           std::uint32_t{AV_SHARED_DONE});
  CHECK_EQ(device, 0x7001u);
  CHECK_EQ(semaphore, 0x7002u);
  std::uint64_t again = 9u;
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x7001u, again), std::uint32_t{AV_SHARED_CONFLICT});
  CHECK_EQ(again, 0u);
  root.close_object(reader, 0x2Au, 0x7003u);
  CHECK_EQ(reader_backend.released_objects(), 0u);
  root.close_object(reader, 0x2Au, 0x7002u);
  CHECK_EQ(reader_backend.released_objects(), 1u);
  root.close_object(reader, 0x2Au, 0x7001u);
  CHECK_EQ(reader_backend.released_objects(), 2u);
  probe.shares_at(1u).end();
  probe.shares_at(0u).end();
  static_cast<void>(reader->release(av::GenerousBudget().budget())); reader->vacate();
  static_cast<void>(maker->release(av::GenerousBudget().budget())); maker->vacate(); }

TEST(executor_root, a_session_is_drained_before_its_shared_objects_are_released)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t maker_block[4096], reader_block[4096];
  MockSessionBackend  maker_backend, reader_backend;
  ContextSession *const maker = &probe.session_at(0u);
  ContextSession *const reader = &probe.session_at(1u);
  CHECK_STATUS(maker->initialize(maker_backend, maker_block, sizeof maker_block), Status::ok);
  CHECK_STATUS(reader->initialize(reader_backend, reader_block, sizeof reader_block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, maker_backend);
  probe.shares_at(1u).bind(*opened.apis().shares(0u), domain, reader_backend);
  probe.remember(0u, AV_EXECUTOR_SUB_API_VULKAN, 0u);
  probe.remember(1u, AV_EXECUTOR_SUB_API_OPENCL, 0u);
  CHECK_EQ(root.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  std::uint64_t made = 0u;
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_DONE});
  root.close(reader);
  CHECK_EQ(reader_backend.released_objects(), 1u);
  CHECK(reader_backend.drains_before_first_release() >= 1u);
  root.close(maker); }

TEST(executor_root, a_session_whose_gpu_work_does_not_drain_in_time_keeps_its_shares_and_slot_until_it_does)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t maker_block[4096], reader_block[4096];
  MockSessionBackend  maker_backend, reader_backend;
  ContextSession *const maker = &probe.session_at(0u);
  ContextSession *const reader = &probe.session_at(1u);
  CHECK_STATUS(maker->initialize(maker_backend, maker_block, sizeof maker_block), Status::ok);
  CHECK_STATUS(reader->initialize(reader_backend, reader_block, sizeof reader_block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, maker_backend);
  probe.shares_at(1u).bind(*opened.apis().shares(0u), domain, reader_backend);
  probe.remember(0u, AV_EXECUTOR_SUB_API_VULKAN, 0u);
  probe.remember(1u, AV_EXECUTOR_SUB_API_OPENCL, 0u);
  CHECK_EQ(root.share_object(maker, 0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), std::uint32_t{AV_SHARED_DONE});
  std::uint64_t made = 0u;
  CHECK_EQ(root.open_object(reader, 0x2Au, AV_SHARED_MEMORY, 4096u, 0u, made), std::uint32_t{AV_SHARED_DONE});

  reader_backend.set_drains_in_time(false);
  root.close(reader);
  CHECK(reader->live());
  CHECK_EQ(reader_backend.released_objects(), 0u);
  CHECK_EQ(reader_backend.shut_downs(), 0u);
  CHECK_EQ(root.live(), 2u);
  CHECK(root.invariants_hold());

  ContextSession *const other = opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
  CHECK(other != nullptr);
  CHECK(other != reader);
  CHECK(reader->live());
  CHECK_EQ(reader_backend.shut_downs(), 0u);

  reader_backend.set_drains_in_time(true);
  ContextSession *const later = opened.held().open(AV_EXECUTOR_SUB_API_VULKAN, intel);
  CHECK(later == reader);
  CHECK_EQ(reader_backend.released_objects(), 1u);
  CHECK_EQ(reader_backend.shut_downs(), 1u);
  root.close(later);
  root.close(other);
  root.close(maker); }

namespace {

class Spread {
public:
    using Job = void (*)(void *context, std::uint32_t index);

    Spread(std::uint32_t threads, Job job, void *context) : threads_(threads), job_(job), context_(context)
    {
        pthread_barrier_init(&gate_, nullptr, threads_);
    }
    ~Spread() { pthread_barrier_destroy(&gate_); }
    Spread(const Spread &) = delete;
    Spread &operator=(const Spread &) = delete;

    void run()
    {
        Worker workers[most];
        pthread_t ids[most];
        for (std::uint32_t index = 0u; index < threads_; index++) {
            workers[index] = {this, index};
            pthread_create(&ids[index], nullptr, &Spread::enter, &workers[index]);
        }
        for (std::uint32_t index = 0u; index < threads_; index++) pthread_join(ids[index], nullptr);
    }

private:
    static constexpr std::uint32_t most = 32u;

    struct Worker {
        Spread       *spread;
        std::uint32_t index;
    };

    static void *enter(void *argument)
    {
        Worker &worker = *static_cast<Worker *>(argument);
        pthread_barrier_wait(&worker.spread->gate_);
        worker.spread->job_(worker.spread->context_, worker.index);
        return nullptr;
    }

    std::uint32_t     threads_;
    Job               job_;
    void             *context_;
    pthread_barrier_t gate_;
};

struct OpenAll;

struct OpenAll {
    ExecutorRoot    *root;
    std::uint32_t    domain;
    std::uint32_t    sub_api;
    ContextSession  *opened[32];
    ContextSession  *shared[32];
    pthread_barrier_t *opened_gate;
    pthread_barrier_t *closing_gate;
};

void open_one(void *context, std::uint32_t index);

class Watcher {
public:
    Watcher(OpenAll *all, std::uint32_t threads) : all_(all), threads_(threads)
    {
        for (std::uint32_t index = 0u; index < threads_; index++) {
            members_[index] = {this, index};
            pthread_create(&ids_[index], nullptr, &Watcher::enter, &members_[index]);
        }
    }
    void join()
    {
        for (std::uint32_t index = 0u; index < threads_; index++) pthread_join(ids_[index], nullptr);
        threads_ = 0u;
    }
    ~Watcher() { join(); }
    Watcher(const Watcher &) = delete;
    Watcher &operator=(const Watcher &) = delete;

private:
    struct Member {
        Watcher      *watcher;
        std::uint32_t index;
    };
    static void *enter(void *argument)
    {
        Member &member = *static_cast<Member *>(argument);
        open_one(member.watcher->all_, member.index);
        return nullptr;
    }

    OpenAll      *all_;
    std::uint32_t threads_;
    Member        members_[32];
    pthread_t     ids_[32];
};

void open_one(void *context, std::uint32_t index)
{
    OpenAll &all = *static_cast<OpenAll *>(context);
    AvContextOptions options = {};
    options.domain = all.domain;
    all.opened[index] = all.root->open(all.sub_api, options, nullptr);
    if (all.opened[index] != nullptr && all.sub_api == AV_EXECUTOR_SUB_API_OPENGL)
        all.shared[index] = all.root->open(all.sub_api, options, all.opened[index]);
    if (all.opened_gate == nullptr) return;
    pthread_barrier_wait(all.opened_gate);
    pthread_barrier_wait(all.closing_gate);
    if (all.shared[index] != nullptr) all.root->close(all.shared[index]);
    if (all.opened[index] != nullptr) all.root->close(all.opened[index]);
}

}

TEST(executor_root_concurrency, contexts_opened_at_once_get_distinct_slots)
{ Opened opened;
  OpenAll all = {&opened.root(), opened.held().domain_of(intel), AV_EXECUTOR_SUB_API_VULKAN, {}, {}, nullptr, nullptr};
  constexpr std::uint32_t threads = 24u;
  for (std::uint32_t round = 0u; round < 20u; round++) {
      all.opened[0] = nullptr;
      for (std::uint32_t index = 0u; index < threads; index++) all.opened[index] = nullptr;
      Spread(threads, open_one, &all).run();
      for (std::uint32_t index = 0u; index < threads; index++) {
          CHECK(all.opened[index] != nullptr);
          for (std::uint32_t other = 0u; other < index; other++) CHECK(all.opened[index] != all.opened[other]);
      }
      CHECK_EQ(opened.root().live(), threads);
      for (std::uint32_t index = 0u; index < threads; index++) opened.root().close(all.opened[index]);
      CHECK_EQ(opened.root().live(), 0u);
  } }

TEST(executor_root_concurrency, opengl_contexts_opened_at_once_never_share_a_memory_group_unless_they_share_a_context)
{ av::SoftwareGl software;
  if (!software.usable()) {
      std::fprintf(stderr, "no llvmpipe EGL device: software GL test skipped\n");
      return;
  }
  MockHostApis apis;
  apis.add_gpu(software.uuid(), AV_EXECUTOR_SUB_API_OPENGL);
  apis.give_gl(software.display());
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  ExecutorRootProbe probe(root);
  OpenAll all = {&root, held.domain_of(software.uuid()), AV_EXECUTOR_SUB_API_OPENGL, {}, {}, nullptr, nullptr};
  constexpr std::uint32_t threads = 8u;
  for (std::uint32_t round = 0u; round < 4u; round++) {
      for (std::uint32_t index = 0u; index < threads; index++) all.opened[index] = all.shared[index] = nullptr;
      pthread_barrier_t opened_gate, closing_gate;
      pthread_barrier_init(&opened_gate, nullptr, threads + 1u);
      pthread_barrier_init(&closing_gate, nullptr, threads + 1u);
      all.opened_gate = &opened_gate;
      all.closing_gate = &closing_gate;
      Watcher watcher(&all, threads);
      pthread_barrier_wait(&opened_gate);
      for (std::uint32_t index = 0u; index < threads; index++) {
          CHECK(all.opened[index] != nullptr);
          CHECK(all.shared[index] != nullptr);
          if (all.opened[index] == nullptr || all.shared[index] == nullptr) continue;
          CHECK_EQ(probe.group_of(*all.opened[index]), probe.group_of(*all.shared[index]));
          for (std::uint32_t other = 0u; other < index; other++)
              if (all.opened[other] != nullptr)
                  CHECK(probe.group_of(*all.opened[index]) != probe.group_of(*all.opened[other]));
      }
      pthread_barrier_wait(&closing_gate);
      watcher.join();
      CHECK_EQ(root.live(), 0u);
      for (std::uint32_t group = 0u; group < ExecutorRoot::sessions; group++) CHECK_EQ(probe.group_refs(group), 0u);
      pthread_barrier_destroy(&opened_gate);
      pthread_barrier_destroy(&closing_gate);
  } }

namespace {

constexpr std::uint32_t wedged_slots = 8u;

class Wedged {
public:
    explicit Wedged(Opened &opened) : probe_(opened.root())
    {
        domain_ = opened.held().domain_of(intel);
        for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) {
            ContextSession &session = probe_.session_at(slot);
            CHECK_STATUS(session.initialize(backends_[slot], blocks_[slot], sizeof blocks_[slot]), Status::ok);
            probe_.shares_at(slot).bind(*opened.apis().shares(0u), domain_, backends_[slot]);
            probe_.remember(slot, AV_EXECUTOR_SUB_API_VULKAN, 0u);
            backends_[slot].set_drains_in_time(false);
            opened.root().close(&session);
        }
    }

    MockSessionBackend &backend(std::uint32_t slot) { return backends_[slot]; }
    void                finish_in_time()
    {
        for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) backends_[slot].set_drains_in_time(true);
    }

private:
    ExecutorRootProbe  probe_;
    std::uint32_t      domain_;
    MockSessionBackend backends_[wedged_slots];
    std::uint8_t       blocks_[wedged_slots][4096];
};

void open_vulkan(void *context, std::uint32_t)
{
    OpenAll &all = *static_cast<OpenAll *>(context);
    AvContextOptions options = {};
    options.domain = all.domain;
    ContextSession *const made = all.root->open(AV_EXECUTOR_SUB_API_VULKAN, options, nullptr);
    if (made != nullptr) all.root->close(made);
}

}

TEST(executor_root_concurrency, a_wedged_slot_reaped_by_contexts_opening_at_once_is_finished_once)
{ for (std::uint32_t round = 0u; round < 20u; round++) {
      Opened opened;
      Wedged wedged(opened);
      CHECK_EQ(opened.root().wedged(), wedged_slots);
      wedged.finish_in_time();
      OpenAll all = {&opened.root(), opened.held().domain_of(intel), AV_EXECUTOR_SUB_API_VULKAN, {}, {}, nullptr, nullptr};
      Spread(16u, open_vulkan, &all).run();
      for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) {
          CHECK_EQ(wedged.backend(slot).shut_downs(), 1u);
          CHECK_EQ(wedged.backend(slot).finished_drains(), 1u);
      }
      CHECK_EQ(opened.root().wedged(), 0u);
      CHECK_EQ(opened.root().live(), 0u);
  } }

TEST(executor_root_concurrency, a_wedged_slot_whose_work_finishes_is_released_without_another_context_opening)
{ Opened opened;
  Wedged wedged(opened);
  CHECK_EQ(opened.root().wedged(), wedged_slots);
  wedged.finish_in_time();
  timespec nap = {0, 50 * 1000 * 1000};
  for (std::uint32_t looks = 0u; looks < 30u && opened.root().wedged() != 0u; looks++) nanosleep(&nap, nullptr);
  CHECK_EQ(opened.root().wedged(), 0u);
  CHECK_EQ(opened.root().live(), 0u);
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) CHECK_EQ(wedged.backend(slot).shut_downs(), 1u); }

TEST(executor_root_concurrency, closing_a_session_drains_its_backend_once_before_releasing_it)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t block[4096];
  MockSessionBackend  backend;
  ContextSession *const session = &probe.session_at(0u);
  CHECK_STATUS(session->initialize(backend, block, sizeof block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, backend);
  probe.remember(0u, AV_EXECUTOR_SUB_API_VULKAN, 0u);
  root.close(session);
  CHECK_EQ(backend.drains(), 1u);
  CHECK_EQ(backend.shut_downs(), 1u); }

namespace {

struct Closing {
    ExecutorRoot   *root;
    ContextSession *session;
};

void *close_session(void *context)
{
    Closing &closing = *static_cast<Closing *>(context);
    closing.root->close(closing.session);
    return nullptr;
}

void nap_for(long milliseconds)
{
    timespec nap = {0, milliseconds * 1000 * 1000};
    nanosleep(&nap, nullptr);
}

}

TEST(executor_root_concurrency, a_closing_slot_cannot_be_claimed_until_its_records_are_cleared)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t block[4096];
  static std::uint8_t other_block[4096];
  MockSessionBackend  backend;
  MockSessionBackend  claimant;
  ContextSession *const session = &probe.session_at(0u);
  CHECK_STATUS(session->initialize(backend, block, sizeof block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, backend);
  probe.remember(0u, AV_EXECUTOR_SUB_API_VULKAN, 0u);
  probe.hold_lock();
  Closing closing = {&root, session};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, close_session, &closing), 0);
  for (std::uint32_t looks = 0u; looks < 2000u && backend.shut_downs() == 0u; looks++) nap_for(1);
  CHECK_EQ(backend.shut_downs(), 1u);
  nap_for(30);
  CHECK_STATUS(probe.session_at(0u).initialize(claimant, other_block, sizeof other_block), Status::invalid_state);
  CHECK(session->live());
  probe.drop_lock();
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(!session->live());
  CHECK_EQ(root.live(), 0u);
  CHECK_STATUS(session->initialize(claimant, other_block, sizeof other_block), Status::ok);
  static_cast<void>(session->release(av::GenerousBudget().budget()));
  session->vacate(); }

namespace {

struct OpenThenClose {
    ExecutorRoot                  *root;
    std::uint32_t                  domain;
    std::atomic<ContextSession *>  session;
    std::atomic<bool>              go;
};

void *open_then_close(void *context)
{
    OpenThenClose &job = *static_cast<OpenThenClose *>(context);
    AvContextOptions options = {};
    options.domain = job.domain;
    ContextSession *const made = job.root->open(AV_EXECUTOR_SUB_API_OPENGL, options, nullptr);
    job.session.store(made);
    while (made != nullptr && !job.go.load()) nap_for(1);
    if (made != nullptr) job.root->close(made);
    return nullptr;
}

}

TEST(executor_root_concurrency, a_closing_opengl_slot_drops_its_group_before_the_slot_can_be_claimed)
{ av::SoftwareGl software;
  if (!software.usable()) {
      std::fprintf(stderr, "no llvmpipe EGL device: software GL test skipped\n");
      return;
  }
  MockHostApis apis;
  apis.add_gpu(software.uuid(), AV_EXECUTOR_SUB_API_OPENGL);
  apis.give_gl(software.display());
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  ExecutorRootProbe probe(root);
  static std::uint8_t other_block[4096];
  MockSessionBackend  claimant;
  OpenThenClose job = {&root, held.domain_of(software.uuid()), {nullptr}, {false}};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, open_then_close, &job), 0);
  for (std::uint32_t looks = 0u; looks < 2000u && job.session.load() == nullptr; looks++) nap_for(1);
  ContextSession *const first = job.session.load();
  CHECK(first != nullptr);
  if (first == nullptr) {
      pthread_join(thread, nullptr);
      return;
  }
  const std::uint32_t group = probe.group_of(*first);
  CHECK_EQ(probe.group_refs(group), 1u);
  probe.hold_lock();
  job.go.store(true);
  nap_for(150);
  CHECK_STATUS(first->initialize(claimant, other_block, sizeof other_block), Status::invalid_state);
  CHECK_EQ(probe.group_refs(group), 1u);
  probe.drop_lock();
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK_EQ(probe.group_refs(group), 0u);
  CHECK_EQ(root.live(), 0u);
  ContextSession *const again = held.open(AV_EXECUTOR_SUB_API_OPENGL, software.uuid());
  CHECK(again != nullptr);
  if (again != nullptr) {
      CHECK_EQ(probe.group_refs(probe.group_of(*again)), 1u);
      root.close(again);
  }
  CHECK(root.invariants_hold()); }

namespace {

struct Churn {
    ExecutorRoot      *root;
    ExecutorRootProbe *probe;
    std::uint32_t      domain;
    std::atomic<std::uint32_t> broken;
};

void churn_one(void *context, std::uint32_t)
{
    Churn &churn = *static_cast<Churn *>(context);
    AvContextOptions options = {};
    options.domain = churn.domain;
    for (std::uint32_t round = 0u; round < 12u; round++) {
        ContextSession *const first = churn.root->open(AV_EXECUTOR_SUB_API_OPENGL, options, nullptr);
        if (first == nullptr) {
            churn.broken++;
            continue;
        }
        ContextSession *const second = churn.root->open(AV_EXECUTOR_SUB_API_OPENGL, options, first);
        if (second == nullptr || churn.probe->group_of(*first) != churn.probe->group_of(*second)) churn.broken++;
        if (second != nullptr) churn.root->close(second);
        churn.root->close(first);
    }
}

}

TEST(executor_root_concurrency, opengl_slots_closing_and_reopening_at_once_keep_every_group_record)
{ av::SoftwareGl software;
  if (!software.usable()) {
      std::fprintf(stderr, "no llvmpipe EGL device: software GL test skipped\n");
      return;
  }
  MockHostApis apis;
  apis.add_gpu(software.uuid(), AV_EXECUTOR_SUB_API_OPENGL);
  apis.give_gl(software.display());
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  ExecutorRootProbe probe(root);
  Churn churn = {&root, &probe, held.domain_of(software.uuid()), {0u}};
  Spread(6u, churn_one, &churn).run();
  CHECK_EQ(churn.broken.load(), 0u);
  CHECK_EQ(root.live(), 0u);
  for (std::uint32_t group = 0u; group < ExecutorRoot::sessions; group++) CHECK_EQ(probe.group_refs(group), 0u);
  CHECK(root.invariants_hold()); }

TEST(executor_root_concurrency, a_failed_close_and_every_failed_reap_let_the_backend_go_of_the_thread)
{ Opened opened;
  Wedged wedged(opened);
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) CHECK(wedged.backend(slot).let_goes() >= 1u);
  std::uint32_t before = 0u;
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) before += wedged.backend(slot).let_goes();
  std::uint32_t after = before;
  for (std::uint32_t looks = 0u; looks < 100u && after <= before; looks++) {
      nap_for(50);
      after = 0u;
      for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) after += wedged.backend(slot).let_goes();
  }
  CHECK(after > before);
  wedged.finish_in_time();
  for (std::uint32_t looks = 0u; looks < 100u && opened.root().wedged() != 0u; looks++) nap_for(50);
  CHECK_EQ(opened.root().wedged(), 0u);
  std::uint32_t settled = 0u;
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) settled += wedged.backend(slot).let_goes();
  opened.root().retry_wedged();
  std::uint32_t last = 0u;
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) last += wedged.backend(slot).let_goes();
  CHECK_EQ(last, settled); }

TEST(executor_root_concurrency, a_closed_session_does_not_let_go_of_the_thread)
{ Opened opened;
  ExecutorRoot       &root = opened.root();
  ExecutorRootProbe   probe(root);
  const std::uint32_t domain = opened.held().domain_of(intel);
  static std::uint8_t block[4096];
  MockSessionBackend  backend;
  ContextSession *const session = &probe.session_at(0u);
  CHECK_STATUS(session->initialize(backend, block, sizeof block), Status::ok);
  probe.shares_at(0u).bind(*opened.apis().shares(0u), domain, backend);
  probe.remember(0u, AV_EXECUTOR_SUB_API_VULKAN, 0u);
  root.close(session);
  CHECK_EQ(backend.shut_downs(), 1u);
  CHECK_EQ(backend.let_goes(), 0u); }

TEST(executor_root_concurrency, the_watcher_stops_at_exit_and_never_reaps_after_it)
{ Opened opened;
  Wedged wedged(opened);
  std::uint32_t baseline = 0u;
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) baseline += wedged.backend(slot).drains();
  std::uint32_t looked = baseline;
  for (std::uint32_t looks = 0u; looks < 80u && looked < baseline + wedged_slots; looks++) {
      nap_for(50);
      looked = 0u;
      for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) looked += wedged.backend(slot).drains();
  }
  CHECK(looked >= baseline + wedged_slots);
  av::WatcherExit::stop_all();
  std::uint32_t stopped = 0u;
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) stopped += wedged.backend(slot).drains();
  nap_for(700);
  opened.root().retry_wedged();
  std::uint32_t later = 0u;
  for (std::uint32_t slot = 0u; slot < wedged_slots; slot++) later += wedged.backend(slot).drains();
  CHECK_EQ(later, stopped);
  CHECK_EQ(opened.root().wedged(), wedged_slots);
  wedged.finish_in_time();
  nap_for(700);
  CHECK_EQ(opened.root().wedged(), wedged_slots); }

TEST(executor_root_open, an_opengl_context_that_fails_after_its_group_was_reserved_gives_the_group_back)
{ MockHostApis apis;
  apis.add_gpu(absent, AV_EXECUTOR_SUB_API_OPENGL);
  RootUnderTest held;
  ExecutorRoot &root = *held;
  CHECK_STATUS(root.initialize(apis), Status::ok);
  ExecutorRootProbe probe(root);
  const std::uint32_t domain = held.domain_of(absent);
  CHECK(domain != 0u);
  CHECK(held.open(AV_EXECUTOR_SUB_API_OPENGL, absent) == nullptr);
  for (std::uint32_t group = 0u; group < ExecutorRoot::sessions; group++) CHECK_EQ(probe.group_refs(group), 0u);
  CHECK_EQ(root.live(), 0u);
  CHECK(root.invariants_hold()); }
