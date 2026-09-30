#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/shared_object_table.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/scheduling/context.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/follow_record.hpp"
#include "core/scheduling/context_table.hpp"
#include "core/scheduling/device_table.hpp"
#include "core/support/status.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void corrupt_live_count(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

struct ContextTableProbe {
    static void corrupt_pool(ContextTable &table) { ObjectPoolProbe::corrupt_live_count(table.contexts_); }
};

}  // namespace av

using av::Adapter;
using av::AllocationTable;
using av::ApertureSegment;
using av::Context;
using av::ContextTable;
using av::DeviceTable;
using av::DxgkargCreateContextRecord;
using av::DxgkargCreateDeviceRecord;
using av::DxgkrnlInterface;
using av::GpuMmuGeometry;
using av::Irql;
using av::MockPlatform;
using av::RvkContextRecord;
using av::Status;
using av::SubApi;
using av::Violation;

namespace {

class Fixture {
public:
    Fixture()
        : interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_), segments_(platform_, segment_),
          shared_(platform_), allocations_(platform_, segment_, segments_, shared_),
          devices_(platform_, allocations_, shared_, interface_), geometry_(platform_), contexts_(platform_, devices_, geometry_),
          device_(nullptr)
    {
        DxgkargCreateDeviceRecord request = {};
        if (devices_.create(adapter_, request) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "device");
        device_ = request.hDevice;
    }

    MockPlatform &platform() { return platform_; }
    DeviceTable  &devices() { return devices_; }
    Adapter      &adapter() { return adapter_; }
    ContextTable &contexts() { return contexts_; }
    void         *device() const { return device_; }

private:
    MockPlatform     platform_;
    DxgkrnlInterface interface_;
    int              pdo_ = 0;
    Adapter          adapter_;
    ApertureSegment  segment_;
    av::MemorySegments            segments_;
    av::SharedObjectTable shared_;
    AllocationTable  allocations_;
    DeviceTable      devices_;
    GpuMmuGeometry   geometry_;
    ContextTable     contexts_;
    void            *device_;
};

}  // namespace

TEST(context_table_create, answers_a_context_on_the_device_for_node_zero)
{ Fixture fixture;
  DxgkargCreateContextRecord request = {};
  request.Flags = 0x1u;   /* SystemContext */
  request.EngineAffinity = 1u;
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  CHECK(fixture.contexts().invariants_hold());
  CHECK_EQ(fixture.contexts().live(), 1u);
  Context *context = fixture.contexts().find(request.hContext);
  CHECK(context != nullptr);
  CHECK(static_cast<void *>(&context->device()) == fixture.device());
  CHECK_EQ(request.ContextInfoDmaBufferSize, 0x10000u); }

TEST(context_table_create, accepts_a_context_with_gpu_virtual_addressing_and_hardware_queues)
{ Fixture fixture;
  DxgkargCreateContextRecord request = {};
  request.Flags = 0x15u;
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  CHECK(fixture.contexts().invariants_hold());
  CHECK_EQ(fixture.contexts().find(request.hContext)->flags(), 0x15u); }

/* Feature 049: the second node is the hardware-scheduled one, and a context
 * lands there only when the process asked for a queue to run it. */
TEST(context_table_create, accepts_a_context_on_the_hardware_scheduled_node)
{ Fixture fixture;
  DxgkargCreateContextRecord request = {};
  request.NodeOrdinal = 1u;
  request.Flags = 0x15u;
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  CHECK(fixture.contexts().invariants_hold());
  CHECK_EQ(fixture.contexts().find(request.hContext)->flags(), 0x15u); }

TEST(context_table_create, refuses_what_this_adapter_cannot_run)
{ for (int knob = 0; knob < 8; knob++) {
      Fixture fixture;
      DxgkargCreateContextRecord request = {};
      void *device = fixture.device();
      if (knob == 0) device = &fixture;                    /* not a device */
      if (knob == 1) request.NodeOrdinal = 1u;              /* node 1 runs queued contexts only */
      if (knob == 7) request.NodeOrdinal = 2u;              /* two nodes */
      if (knob == 2) request.EngineAffinity = 2u;          /* one engine */
      if (knob == 3) request.Flags = 0x8u;                 /* SystemProtectedContext: WDDM 2.1 */
      if (knob == 4) request.PrivateDriverDataSize = 4u;   /* no private data defined */
      if (knob == 5) fixture.platform().fail_allocation_at(1u);
      if (knob == 6) request.Flags = 0x10u;               /* HwQueueSupported needs VirtualAddressing */
      const Status status = fixture.contexts().create(device, request);
      CHECK(status == (knob == 5 ? Status::no_memory : Status::invalid_parameter));
      CHECK_EQ(fixture.contexts().live(), 0u);
      CHECK(request.hContext == nullptr);
  } }

TEST(context_table_destroy, releases_the_context_once)
{ Fixture fixture;
  DxgkargCreateContextRecord request = {};
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  CHECK_STATUS(fixture.contexts().destroy(request.hContext), Status::ok);
  CHECK(fixture.contexts().invariants_hold());
  CHECK_STATUS(fixture.contexts().destroy(request.hContext), Status::invalid_parameter);
  CHECK(fixture.contexts().find(request.hContext) == nullptr); }

TEST(context_table, is_passive_only)
{ Fixture fixture;
  DxgkargCreateContextRecord request = {};
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.contexts().destroy(request.hContext));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {} }

TEST(context_table_invariants, a_broken_pool_is_detected)
{ Fixture fixture;
  av::ContextTableProbe::corrupt_pool(fixture.contexts());
  CHECK(!fixture.contexts().invariants_hold()); }

namespace {

/* The private data a process sends to ask for an rvk context, encoded. */
std::size_t rvk_private_data(std::uint8_t *bytes, std::size_t capacity)
{ RvkContextRecord record(SubApi::vulkan, 16u, 2u, 4096u);
  if (record.encode(bytes, capacity) != Status::ok) return 0u;
  return RvkContextRecord::encoded_size; }

}  // namespace

/* A CreateContext carrying a valid record builds an rvk context that remembers
 * what it asked for; one carrying no private data is a plain context as before. */
TEST(context_table_create, a_valid_record_makes_an_rvk_context)
{ Fixture fixture;
  std::uint8_t data[RvkContextRecord::encoded_size] = {};
  DxgkargCreateContextRecord request = {};
  request.Flags = 0x4u;   /* VirtualAddressing */
  request.pPrivateDriverData = data;
  request.PrivateDriverDataSize = static_cast<std::uint32_t>(rvk_private_data(data, sizeof data));
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  Context *context = fixture.contexts().find(request.hContext);
  CHECK(context != nullptr);
  CHECK(context->is_rvk());
  CHECK_EQ(context->rvk_record().sub_api(), SubApi::vulkan);
  CHECK_EQ(context->rvk_record().ring(), 2u);
  /* Its submissions carry their descriptor as private data, which VidSch
   * refuses past the size the context reports (DXGCONTEXT::SubmitCommand,
   * dxgkrnl.sys @1402d7720). */
  CHECK_EQ(request.ContextInfoDmaBufferPrivateDataSize, 32u);
  CHECK(fixture.contexts().invariants_hold()); }

TEST(context_table_create, a_context_with_no_private_data_is_not_rvk)
{ Fixture fixture;
  DxgkargCreateContextRecord request = {};
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::ok);
  CHECK(!fixture.contexts().find(request.hContext)->is_rvk());
  /* Room for a FollowRecord, so dxgkrnl hands every packet's private data on
   * (Adapter::submit_command_virtual). */
  CHECK_EQ(request.ContextInfoDmaBufferPrivateDataSize, av::FollowRecord::encoded_size); }

/* Private data the size of a record but not a valid one is refused, and no
 * context is left behind. */
TEST(context_table_create, a_record_sized_private_data_that_does_not_decode_is_refused)
{ Fixture fixture;
  std::uint8_t data[RvkContextRecord::encoded_size] = {};
  rvk_private_data(data, sizeof data);
  data[0] ^= 0xFFu;   /* the magic */
  DxgkargCreateContextRecord request = {};
  request.Flags = 0x4u;
  request.pPrivateDriverData = data;
  request.PrivateDriverDataSize = RvkContextRecord::encoded_size;
  CHECK_STATUS(fixture.contexts().create(fixture.device(), request), Status::invalid_parameter);
  CHECK_EQ(fixture.contexts().live(), 0u);
  CHECK(request.hContext == nullptr); }

TEST(context_table_report, writes_each_context_and_its_device)
{ Fixture fixture;
  DxgkargCreateContextRecord first = {};
  first.Flags = 0x1u;
  first.EngineAffinity = 1u;
  CHECK_STATUS(fixture.contexts().create(fixture.device(), first), Status::ok);
  DxgkargCreateContextRecord second = {};
  second.EngineAffinity = 1u;
  CHECK_STATUS(fixture.contexts().create(fixture.device(), second), Status::ok);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.contexts().report(writer);
  const auto hex_of = [](const void *object) {
      return static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(object));
  };
  char expected[512];
  std::snprintf(expected, sizeof expected,
                "context_table/0 busy=false live=2\n"
                "context_table/0/context/0 handle=0x%016llX device=0x%016llX node=0 flags=0x0000000000000001 rvk=false\n"
                "context_table/0/context/1 handle=0x%016llX device=0x%016llX node=0 flags=0x0000000000000000 rvk=false",
                hex_of(first.hContext), hex_of(fixture.device()), hex_of(second.hContext), hex_of(fixture.device()));
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  fixture.platform().hold_elsewhere(av::LockRank::context_table);
  av::StateWriter busy(bytes, sizeof bytes);
  fixture.contexts().report(busy);
  CHECK(std::strcmp(busy.dump(), "context_table/0 busy=true") == 0);
  fixture.platform().set_irql(av::Irql::dispatch);
  fixture.contexts().report(busy);
  CHECK(fixture.platform().claim(av::Violation::irql_ceiling));
  CHECK(fixture.platform().claim(av::Violation::irql_ceiling));
  fixture.platform().set_irql(av::Irql::passive); }

namespace {

/* A context on `device`: an rvk one whose ring is allocation 2 when `rvk`. */
void *context_on(Fixture &fixture, void *device, bool rvk)
{ std::uint8_t data[RvkContextRecord::encoded_size] = {};
  DxgkargCreateContextRecord request = {};
  request.Flags = 0x4u;
  if (rvk) {
      request.pPrivateDriverData = data;
      request.PrivateDriverDataSize = static_cast<std::uint32_t>(rvk_private_data(data, sizeof data));
  }
  CHECK_STATUS(fixture.contexts().create(device, request), Status::ok);
  return request.hContext; }

void *device_in(Fixture &fixture, void *process)
{ DxgkargCreateDeviceRecord request = {};
  request.hKmdProcess = process;
  CHECK_STATUS(fixture.devices().create(fixture.adapter(), request), Status::ok);
  return request.hDevice; }

}  // namespace

/* A packet on an ordinary context that carries a FollowRecord waits for the
 * rvk context whose ring the record names - its process's own, since a handle
 * names an allocation only in the process that made it. */
TEST(context_table_followed, finds_the_rvk_context_of_the_same_process_by_its_ring)
{ Fixture fixture;
  void *const rvk = context_on(fixture, fixture.device(), true);
  const Context &follower = *fixture.contexts().find(context_on(fixture, fixture.device(), false));
  CHECK(fixture.contexts().followed(follower, 2u) == fixture.contexts().find(rvk));
  CHECK(fixture.contexts().followed(follower, 3u) == nullptr);
  CHECK(fixture.contexts().invariants_hold()); }

TEST(context_table_followed, another_process_s_ring_is_never_followed)
{ Fixture fixture;
  int other_process = 0;
  static_cast<void>(context_on(fixture, fixture.device(), true));
  void *const elsewhere = device_in(fixture, &other_process);
  const Context &follower = *fixture.contexts().find(context_on(fixture, elsewhere, false));
  CHECK(fixture.contexts().followed(follower, 2u) == nullptr);
  /* The same ring in the follower's own process is found. */
  void *const own = context_on(fixture, elsewhere, true);
  CHECK(fixture.contexts().followed(follower, 2u) == fixture.contexts().find(own)); }

/* An ordinary context has no ring, whatever its record reads as. */
TEST(context_table_followed, an_ordinary_context_is_never_followed)
{ Fixture fixture;
  const Context &follower = *fixture.contexts().find(context_on(fixture, fixture.device(), false));
  static_cast<void>(context_on(fixture, fixture.device(), false));
  CHECK(fixture.contexts().followed(follower, follower.rvk_record().ring()) == nullptr); }
