#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/context_table.hpp"
#include "core/scheduling/device_table.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

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
using av::Status;
using av::Violation;

namespace {

class Fixture {
public:
    Fixture()
        : interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_), host_visible_(platform_), segments_(platform_, segment_, host_visible_),
          allocations_(platform_, segment_, segments_),
          devices_(platform_, allocations_, interface_), geometry_(platform_), contexts_(platform_, devices_, geometry_),
          device_(nullptr)
    {
        DxgkargCreateDeviceRecord request = {};
        if (devices_.create(adapter_, request) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "device");
        device_ = request.hDevice;
    }

    MockPlatform &platform() { return platform_; }
    DeviceTable  &devices() { return devices_; }
    ContextTable &contexts() { return contexts_; }
    void         *device() const { return device_; }

private:
    MockPlatform     platform_;
    DxgkrnlInterface interface_;
    int              pdo_ = 0;
    Adapter          adapter_;
    ApertureSegment  segment_;
    av::HostVisibleSegment        host_visible_;
    av::MemorySegments            segments_;
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

TEST(context_table_create, refuses_what_this_adapter_cannot_run)
{ for (int knob = 0; knob < 7; knob++) {
      Fixture fixture;
      DxgkargCreateContextRecord request = {};
      void *device = fixture.device();
      if (knob == 0) device = &fixture;                    /* not a device */
      if (knob == 1) request.NodeOrdinal = 1u;             /* one node */
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
