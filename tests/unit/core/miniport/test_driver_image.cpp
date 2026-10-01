#include "core/ddi/entry_points.hpp"
#include "core/miniport/ddi_table.hpp"
#include "core/miniport/driver_image.hpp"
#include "core/support/status.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/scope_binding.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/log_record.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/service_value.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::DriverImage;
using av::FakeDxgkrnl;
using av::ImageRange;
using av::MockTraceHost;
using av::MockPlatform;
using av::ServiceValue;
using av::Status;

namespace av {

template <>
struct DriverImageProbe<MockPlatform, MockTraceHost> {
    explicit DriverImageProbe(DriverImage<MockPlatform, MockTraceHost> &target) : image(target) {}
    Status occupy_miniport_storage(MockPlatform &platform) { return image.miniport_.construct(platform); }
    void   release_miniport_storage() { image.miniport_.destroy(); }
    bool   platform_kept() const { return image.platform_.constructed(); }
    DriverImage<MockPlatform, MockTraceHost> &image;
};

}  // namespace av

using av::DriverImageProbe;
using Image = DriverImage<MockPlatform, MockTraceHost>;

namespace {

int driver_object = 0;
int registry_path = 0;
/* The service value that asks for the trace table; without it there is none. */
const ServiceValue trace_scopes("TraceScopes", 1u);

/* An image lives in static storage in the kernel; here each test gets one and
 * tears it down the way the shim never has to. */
class Entered {
public:
    ~Entered() { image_.leave(); }
    Image &image() { return image_; }
    /* DriverEntry with no image range and nothing to run at a bugcheck. */
    std::int32_t enter() { return image_.enter(&driver_object, &registry_path, ImageRange(0u, 0u), nullptr); }
    /* The same, with the service asking for the trace table. */
    std::int32_t enter_traced()
    {
        return image_.enter(&driver_object, &registry_path, ImageRange(0u, 0u), nullptr, 0u, trace_scopes);
    }

private:
    Image image_;
};

}  // namespace

TEST(driver_image_enter, registers_the_miniport_table_and_binds_the_root)
{ Entered entered;
  CHECK_EQ(entered.enter(), 0);
  MockPlatform &platform = entered.image().platform();
  CHECK(platform.registered_driver_object() == &driver_object);
  CHECK(platform.registered_registry_path() == &registry_path);
  CHECK(av::miniport_binding.miniport() != nullptr);
  FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(platform.registered_table()), Status::ok);
  CHECK_EQ(platform.logged(), 1u); }   /* DriverEntry; no trace table was asked for */

/* The table is up before the miniport exists, its address is where the host
 * reads it from (docs/trace.md's line, which tools/mcp/av_inspect.py parses),
 * and it is gone once the image is left. */
TEST(driver_image_enter, lays_the_trace_table_out_first_and_logs_where_it_is)
{ Entered entered;
  CHECK_EQ(entered.enter_traced(), 0);
  MockPlatform &platform = entered.image().platform();
  CHECK(platform.logged_line("avgpu: trace table at physical 0x0000000100000000 bytes 52832\n"));
  CHECK(av::scope_binding.table() != nullptr);
  CHECK(!platform.bugcheck_registered());
  entered.image().leave();
  CHECK(av::scope_binding.table() == nullptr); }

/* Every scope costs the DPC it runs in, and dxgkrnl's other DPCs spin on the
 * lock that DPC holds: a service that does not ask for the table gets none,
 * and nothing is allocated for it. */
TEST(driver_image_enter, keeps_no_trace_table_unless_the_service_asks)
{ Entered entered;
  CHECK_EQ(entered.enter(), 0);
  MockPlatform &platform = entered.image().platform();
  CHECK(av::scope_binding.table() == nullptr);
  CHECK(!platform.logged_line("trace table"));
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(av::miniport_binding.miniport() != nullptr); }

TEST(driver_image_enter, a_trace_scopes_value_of_zero_keeps_none)
{ Entered entered;
  const ServiceValue off("TraceScopes", 0u);
  CHECK_EQ(entered.image().enter(&driver_object, &registry_path, ImageRange(0u, 0u), nullptr, 0u, off), 0);
  CHECK(av::scope_binding.table() == nullptr); }

/* The refusal releases the platform that logged it, so the line is asserted
 * through a record that outlives it. Without this line a driver that dxgkrnl
 * refuses says only "DriverEntry" and stops, which is what the first live run
 * of feature 036 left us reading. */
TEST(driver_image_enter, a_registration_dxgkrnl_refuses_unbinds_and_logs_its_status)
{ av::LogRecord record;
  Entered entered;
  CHECK_EQ(static_cast<std::uint32_t>(
               entered.image().enter(&driver_object, &registry_path, ImageRange(0u, 0u), nullptr, 0xC0000059u, record)),
           0xC0000059u);
  CHECK(av::miniport_binding.miniport() == nullptr);
  CHECK_EQ(record.count(), 2u);   /* DriverEntry, then the refusal: no trace table was asked for */
  CHECK(std::strstr(record.last(), "DxgkInitialize refused") != nullptr);
  CHECK(std::strstr(record.last(), "C0000059") != nullptr); }

TEST(driver_image_enter, a_second_entry_is_refused)
{ Entered entered;
  CHECK_EQ(entered.enter(), 0);
  CHECK_EQ(static_cast<std::uint32_t>(entered.enter()), 0xC0000184u); }

TEST(driver_image_enter, occupied_root_storage_is_refused)
{ Entered entered;
  MockPlatform scratch;
  DriverImageProbe<MockPlatform, MockTraceHost> probe(entered.image());
  CHECK_STATUS(probe.occupy_miniport_storage(scratch), Status::ok);
  CHECK_EQ(static_cast<std::uint32_t>(entered.enter()), 0xC0000184u);
  CHECK(av::miniport_binding.miniport() == nullptr);
  CHECK(av::scope_binding.table() == nullptr);
  probe.release_miniport_storage(); }

/* DxgkDdiUnload is the image's end, not only its adapters': the platform - its
 * log thread - and the trace table go with it, before dxgkrnl unmaps the code
 * they run. A live driver upgrade bugchecked 0xCE in the log thread when they
 * stayed. */
TEST(driver_image_leave, unload_leaves_the_whole_image)
{ Entered entered;
  CHECK_EQ(entered.enter_traced(), 0);
  int pdo = 0; void *context = nullptr;
  CHECK_EQ(AvDdiAddDevice(&pdo, &context), 0);
  DriverImageProbe<MockPlatform, MockTraceHost> probe(entered.image());
  CHECK(probe.platform_kept());
  AvDdiUnload();
  CHECK(av::miniport_binding.miniport() == nullptr);
  CHECK(!probe.platform_kept());
  /* Nothing is bound to leave twice. */
  AvDdiUnload(); }

TEST(driver_image_leave, unbinds_and_releases_everything)
{ Entered entered;
  CHECK_EQ(entered.enter(), 0);
  int pdo = 0; void *context = nullptr;
  CHECK_EQ(AvDdiAddDevice(&pdo, &context), 0);
  entered.image().leave();
  CHECK(av::miniport_binding.miniport() == nullptr);
  entered.image().leave(); }

namespace {

Image *bugchecking = nullptr;

void run_bugcheck(void *, std::uint32_t) { bugchecking->bugcheck(); }

}  // namespace

/* A bugcheck writes the table - with the frames the routine itself runs in -
 * to the debug port, and the dump keeps the table's bytes. Every name is read,
 * since the whole address space stands for the driver's image here. */
TEST(driver_image_bugcheck, writes_the_table_through_the_registered_routine)
{ Entered entered;
  bugchecking = &entered.image();
  CHECK_EQ(entered.image().enter(&driver_object, &registry_path, ImageRange(0u, 0xFFFFFFFFFFFFFFFFull),
                                 &run_bugcheck, 0u, trace_scopes), 0);
  MockPlatform &platform = entered.image().platform();
  CHECK(platform.bugcheck_registered());
  CHECK_EQ(platform.bugcheck_bytes(), 52832u);
  CHECK(platform.bugcheck_buffer() == static_cast<const void *>(av::scope_binding.table()->region()));
  CHECK(platform.bugcheck());
  CHECK(platform.bugcheck_line("avgpu: trace module=avgpu pid=0 slots=64\n"));
  CHECK(platform.bugcheck_line("avgpu: thread 100 active depth=2 "));
  CHECK(platform.bugcheck_line("avgpu:   #0 DriverImage::bugcheck irql=0 for_ms=0\n"));
  CHECK(platform.bugcheck_line("avgpu:   #1 DriverTrace::bugcheck irql=0 for_ms=0\n"));
  CHECK_EQ(platform.bugcheck_lines(), 4u);
  entered.image().leave();
  bugchecking = nullptr; }

TEST(driver_image_bugcheck, without_an_image_entered_writes_nothing)
{ Image image;
  image.bugcheck();
  CHECK(av::scope_binding.table() == nullptr); }
