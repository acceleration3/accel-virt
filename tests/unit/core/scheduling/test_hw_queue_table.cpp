#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/context_table.hpp"
#include "core/scheduling/device_table.hpp"
#include "core/scheduling/hw_queue.hpp"
#include "core/scheduling/hw_queue_table.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>

using av::Adapter;
using av::AllocationTable;
using av::ApertureSegment;
using av::ContextTable;
using av::DeviceTable;
using av::DxgkargCreateContextRecord;
using av::DxgkargCreateDeviceRecord;
using av::DxgkargCreateHwQueueRecord;
using av::DxgkrnlInterface;
using av::GpuMmuGeometry;
using av::HwQueue;
using av::HwQueueTable;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void corrupt_live_count(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

struct HwQueueTableProbe {
    static void corrupt_pool(HwQueueTable &table) { ObjectPoolProbe::corrupt_live_count(table.queues_); }
};

}  // namespace av

namespace {

class Fixture {
public:
    Fixture()
        : interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_), host_visible_(platform_), segments_(platform_, segment_, host_visible_),
          allocations_(platform_, segment_, segments_),
          devices_(platform_, allocations_, interface_), geometry_(platform_), contexts_(platform_, devices_, geometry_),
          queues_(platform_, contexts_), fence_(0u), hw_context_(context_with(0x14u)), plain_context_(context_with(0x4u))
    {
    }

    MockPlatform &platform() { return platform_; }
    HwQueueTable &queues() { return queues_; }
    void         *hw_context() const { return hw_context_; }
    void         *plain_context() const { return plain_context_; }
    DxgkargCreateHwQueueRecord request()
    {
        DxgkargCreateHwQueueRecord record = {};
        record.hHwQueue = &fence_;
        record.Flags = 0x7u;
        record.HwQueueProgressFenceCPUVirtualAddress = &fence_;
        return record;
    }

private:
    void *context_with(std::uint32_t flags)
    {
        DxgkargCreateDeviceRecord device = {};
        DxgkargCreateContextRecord context = {};
        context.Flags = flags;
        if (devices_.create(adapter_, device) != Status::ok || contexts_.create(device.hDevice, context) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "context");
        return context.hContext;
    }

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
    HwQueueTable     queues_;
    std::uint64_t    fence_;
    void            *hw_context_;
    void            *plain_context_;
};

}  // namespace

TEST(hw_queue_table_create, answers_a_queue_on_a_hardware_queue_context)
{ Fixture fixture;
  DxgkargCreateHwQueueRecord request = fixture.request();
  CHECK_STATUS(fixture.queues().create(fixture.hw_context(), request), Status::ok);
  CHECK(fixture.queues().invariants_hold());
  CHECK_EQ(fixture.queues().live(), 1u);
  CHECK(static_cast<void *>(&static_cast<HwQueue *>(request.hHwQueue)->context()) == fixture.hw_context());
  CHECK(fixture.queues().serves(fixture.hw_context()));
  CHECK(!fixture.queues().serves(fixture.plain_context()));
  CHECK_STATUS(fixture.queues().destroy(request.hHwQueue), Status::ok);
  CHECK(fixture.queues().invariants_hold());
  CHECK(!fixture.queues().serves(fixture.hw_context()));
  CHECK_STATUS(fixture.queues().destroy(request.hHwQueue), Status::invalid_parameter); }

TEST(hw_queue_table_create, refuses_what_this_driver_cannot_run)
{ for (int knob = 0; knob < 6; knob++) {
      Fixture fixture;
      DxgkargCreateHwQueueRecord request = fixture.request();
      void *context = fixture.hw_context();
      if (knob == 0) context = &fixture;                                    /* not a context */
      if (knob == 1) context = fixture.plain_context();                     /* no HwQueueSupported */
      if (knob == 2) request.Flags = 0x8u;                                  /* NoKmdAccess */
      if (knob == 3) request.PrivateDriverDataSize = 4u;                    /* no private data defined */
      if (knob == 4) request.HwQueueProgressFenceCPUVirtualAddress = nullptr;
      if (knob == 5) fixture.platform().fail_allocation_at(1u);
      const Status status = fixture.queues().create(context, request);
      CHECK(status == (knob == 5 ? Status::no_memory : Status::invalid_parameter));
      CHECK_EQ(fixture.queues().live(), 0u);
  } }

TEST(hw_queue_table_invariants, a_broken_pool_is_detected)
{ Fixture fixture;
  av::HwQueueTableProbe::corrupt_pool(fixture.queues());
  CHECK(!fixture.queues().invariants_hold()); }

TEST(hw_queue_table, is_passive_only)
{ Fixture fixture;
  DxgkargCreateHwQueueRecord request = fixture.request();
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.queues().create(fixture.hw_context(), request));
  static_cast<void>(fixture.queues().destroy(nullptr));
  static_cast<void>(fixture.queues().serves(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {} }

TEST(hw_queue_table_report, writes_each_queue_and_its_context)
{ Fixture fixture;
  DxgkargCreateHwQueueRecord request = fixture.request();
  CHECK_STATUS(fixture.queues().create(fixture.hw_context(), request), Status::ok);
  char bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.queues().report(writer);
  char expected[256];
  std::snprintf(expected, sizeof expected,
                "hw_queue_table/0 busy=false live=1\n"
                "hw_queue_table/0/hw_queue/0 handle=0x%016llX context=0x%016llX",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(request.hHwQueue)),
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(fixture.hw_context())));
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  fixture.platform().hold_elsewhere(av::LockRank::hw_queue_table);
  av::StateWriter busy(bytes, sizeof bytes);
  fixture.queues().report(busy);
  CHECK(std::strcmp(busy.dump(), "hw_queue_table/0 busy=true") == 0);
  fixture.platform().set_irql(av::Irql::dispatch);
  fixture.queues().report(busy);
  CHECK(fixture.platform().claim(av::Violation::irql_ceiling));
  CHECK(fixture.platform().claim(av::Violation::irql_ceiling));
  fixture.platform().set_irql(av::Irql::passive);
  fixture.platform().released_elsewhere();
  CHECK_STATUS(fixture.queues().destroy(request.hHwQueue), Status::ok); }

/* Feature 049: a doorbell is created on the queue dxgkrnl names, so the table
 * is asked for queues by handle - including handles nobody made. */
TEST(hw_queue_table, finds_the_queue_a_handle_names_and_nothing_for_one_nobody_made)
{ Fixture fixture;
  DxgkargCreateHwQueueRecord request = fixture.request();
  CHECK_STATUS(fixture.queues().create(fixture.hw_context(), request), Status::ok);
  CHECK(fixture.queues().find(request.hHwQueue) != nullptr);
  CHECK(&fixture.queues().find(request.hHwQueue)->context() == fixture.hw_context());
  int stack = 0;
  CHECK(fixture.queues().find(&stack) == nullptr);
  CHECK(fixture.queues().find(nullptr) == nullptr);
  CHECK_STATUS(fixture.queues().destroy(request.hHwQueue), Status::ok);
  CHECK(fixture.queues().find(request.hHwQueue) == nullptr); }
