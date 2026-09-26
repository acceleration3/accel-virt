/* The adapter's doorbells (feature 049, US1 and US2): one per hardware queue a
 * process asked for, each naming the device page that queue's context was
 * bound to, connected or not.
 *
 * A doorbell is the one object in the driver whose whole purpose is to be used
 * without the driver: once connected, the process stores to the page and the
 * kernel never hears of it again. So what these tests judge is the states
 * around that - who may have one, what a disconnect leaves, and what a process
 * would read out of its status.
 */
#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_doorbell_records.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/shared_object_table.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/pci/mapped_register_window.hpp"
#include "core/scheduling/context_table.hpp"
#include "core/scheduling/device_table.hpp"
#include "core/scheduling/doorbell_table.hpp"
#include "core/scheduling/hw_queue.hpp"
#include "core/scheduling/queue_doorbell.hpp"
#include "core/scheduling/hw_queue_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include "core/trace/state_writer.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::DoorbellStatus;
using av::HwQueue;
using av::QueueDoorbell;
using av::DoorbellTable;
using av::MappedRegisterWindow;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint32_t page_bytes = 4096u;
constexpr std::uint32_t pages = 64u;

/* A doorbell BAR of `pages` pages as the adapter maps one, and two hardware
 * queues to hold doorbells on. */
class Doorbells {
public:
    Doorbells()
        : interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_), segments_(platform_, segment_), shared_(platform_), allocations_(platform_, segment_, segments_, shared_),
          devices_(platform_, allocations_, shared_, interface_), geometry_(platform_),
          contexts_(platform_, devices_, geometry_), queues_(platform_, contexts_),
          window_(platform_, bytes_, sizeof bytes_), table_(platform_, window_),
          first_(make_queue()), second_(make_queue())
    {
    }
    Doorbells(const Doorbells &) = delete;
    Doorbells &operator=(const Doorbells &) = delete;

    DoorbellTable &table() { return table_; }
    MockPlatform  &platform() { return platform_; }
    HwQueue       &first() { return *first_; }
    HwQueue       &second() { return *second_; }
    /* Where page `index` begins in this mapping: what a connect answers. */
    const void    *page(std::uint32_t index) { return window_.address_at(index * page_bytes); }

private:
    HwQueue *make_queue()
    {
        av::DxgkargCreateDeviceRecord device = {};
        av::DxgkargCreateContextRecord context = {};
        /* A context made for hardware queues, which is the only kind a queue
         * runs on. */
        context.Flags = 0x14u;
        av::DxgkargCreateHwQueueRecord queue = {};
        queue.HwQueueProgressFenceCPUVirtualAddress = &fence_;
        if (devices_.create(adapter_, device) != Status::ok ||
            contexts_.create(device.hDevice, context) != Status::ok ||
            queues_.create(context.hContext, queue) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "queue");
        return queues_.find(queue.hHwQueue);
    }

    MockPlatform           platform_;
    av::DxgkrnlInterface   interface_;
    int                    pdo_ = 0;
    av::Adapter            adapter_;
    av::ApertureSegment    segment_;
    av::MemorySegments     segments_;
    av::SharedObjectTable shared_;
    av::AllocationTable    allocations_;
    av::DeviceTable        devices_;
    av::GpuMmuGeometry     geometry_;
    av::ContextTable       contexts_;
    av::HwQueueTable       queues_;
    std::uint64_t          fence_ = 0u;
    std::uint8_t           bytes_[pages * page_bytes] = {};
    MappedRegisterWindow   window_;
    DoorbellTable          table_;
    HwQueue               *first_;
    HwQueue               *second_;
};

}  // namespace

TEST(doorbell_table, a_doorbell_connected_answers_the_page_its_context_was_bound_to)
{ Doorbells held;
  CHECK(held.table().invariants_hold());
  CHECK_EQ(held.table().live(), 0u);
  void *made = nullptr;
  CHECK_STATUS(held.table().create(held.first(), 3u, made), Status::ok);
  CHECK(made != nullptr);
  CHECK_EQ(held.table().live(), 1u);
  CHECK(held.table().invariants_hold());

  /* Created is not connected: a process that never connected has nowhere to
   * store, and the status says which way to submit. */
  CHECK(held.table().status_of(made) == DoorbellStatus::disconnected_retry);
  void          *address = nullptr;
  DoorbellStatus status = DoorbellStatus::disconnected_abort;
  HwQueue       *queue = nullptr;
  CHECK_STATUS(held.table().connect(made, address, status, queue), Status::ok);
  CHECK(address == held.page(3u));
  CHECK(status == DoorbellStatus::connected);
  /* The queue it hangs off, which is what tells the driver whose context has
   * moved its submissions into user mode. */
  CHECK(queue == &held.first());
  CHECK(held.table().status_of(made) == DoorbellStatus::connected);
  CHECK(held.table().invariants_hold());

  /* Disconnected, it answers no address at all, and a process reading its
   * status is told to submit the other way (research.md §10). */
  CHECK_STATUS(held.table().disconnect(made), Status::ok);
  CHECK(held.table().status_of(made) == DoorbellStatus::disconnected_retry);
  void *again = nullptr;
  CHECK_STATUS(held.table().connect(made, again, status, queue), Status::ok);
  CHECK(again == held.page(3u));
  CHECK(status == DoorbellStatus::connected);

  /* The handle is the QueueDoorbell itself, as a hardware queue's handle is
   * its HwQueue: what dxgkrnl passes back is what the driver made, and the
   * queue it names is what tells the driver whose context has moved its
   * submissions into user mode (feature 049). */
  CHECK(&static_cast<QueueDoorbell *>(made)->queue() == &held.first());
  CHECK(held.table().queue_of(made) == &held.first());
  int outside = 0;
  CHECK(held.table().queue_of(&outside) == nullptr);
  CHECK(held.table().queue_of(nullptr) == nullptr);
  CHECK_EQ(static_cast<QueueDoorbell *>(made)->page(), 3u);

  CHECK_STATUS(held.table().destroy(made), Status::ok);
  CHECK_EQ(held.table().live(), 0u);
  CHECK(held.table().invariants_hold()); }

/* Everything a doorbell is refused for. */
TEST(doorbell_table, a_doorbell_nobody_made_is_no_doorbell)
{ Doorbells held;
  void     *made = nullptr;
  /* A page past the BAR is a page no store could reach. */
  CHECK_STATUS(held.table().create(held.first(), pages, made), Status::invalid_parameter);
  CHECK_STATUS(held.table().create(held.first(), pages + 4u, made), Status::invalid_parameter);
  CHECK_EQ(held.table().live(), 0u);

  CHECK_STATUS(held.table().create(held.first(), 0u, made), Status::ok);
  void          *address = nullptr;
  DoorbellStatus status = DoorbellStatus::connected;
  int            stack = 0;
  HwQueue       *queue = &held.first();
  CHECK_STATUS(held.table().connect(&stack, address, status, queue), Status::invalid_parameter);
  CHECK(queue == nullptr);
  CHECK_STATUS(held.table().disconnect(&stack), Status::invalid_parameter);
  CHECK_STATUS(held.table().destroy(&stack), Status::invalid_parameter);
  CHECK_STATUS(held.table().connect(nullptr, address, status, queue), Status::invalid_parameter);
  /* And a handle destroyed is one nobody made. */
  CHECK_STATUS(held.table().destroy(made), Status::ok);
  CHECK_STATUS(held.table().destroy(made), Status::invalid_parameter);
  CHECK_STATUS(held.table().connect(made, address, status, queue), Status::invalid_parameter);
  CHECK(held.table().status_of(made) == DoorbellStatus::disconnected_abort);
  CHECK(held.table().invariants_hold()); }

/* A queue destroyed takes its doorbells with it: dxgkrnl destroys the queue
 * and a doorbell that outlived it would name a page of a context that is
 * gone. */
TEST(doorbell_table, a_queue_destroyed_takes_its_doorbells)
{ Doorbells held;
  void     *first = nullptr;
  void     *second = nullptr;
  void     *other = nullptr;
  CHECK_STATUS(held.table().create(held.first(), 1u, first), Status::ok);
  CHECK_STATUS(held.table().create(held.first(), 1u, second), Status::ok);
  CHECK_STATUS(held.table().create(held.second(), 2u, other), Status::ok);
  CHECK(first != second);
  CHECK_EQ(held.table().live(), 3u);
  CHECK(held.table().serves(&held.first()));

  held.table().forget_queue(&held.first());
  CHECK_EQ(held.table().live(), 1u);
  CHECK(!held.table().serves(&held.first()));
  CHECK(held.table().serves(&held.second()));
  CHECK(held.table().status_of(first) == DoorbellStatus::disconnected_abort);
  /* The other queue's doorbell is untouched. */
  void          *address = nullptr;
  DoorbellStatus status = DoorbellStatus::disconnected_abort;
  HwQueue       *queue = nullptr;
  CHECK_STATUS(held.table().connect(other, address, status, queue), Status::ok);
  CHECK(queue == &held.second());
  CHECK(address == held.page(2u));
  /* Forgetting a queue with no doorbells, and the null one, is no-one's
   * error. */
  held.table().forget_queue(&held.first());
  held.table().forget_queue(nullptr);
  CHECK_EQ(held.table().live(), 1u);
  CHECK(held.table().invariants_hold()); }

/* A table with no room refuses rather than handing out a doorbell that names
 * someone else's page. */
TEST(doorbell_table, a_table_with_no_room_refuses)
{ Doorbells held;
  void     *made = nullptr;
  for (std::uint32_t taken = 0u; taken < DoorbellTable::max_doorbells; taken++)
      CHECK_STATUS(held.table().create(held.first(), taken % pages, made), Status::ok);
  CHECK_EQ(held.table().live(), DoorbellTable::max_doorbells);
  void *past = nullptr;
  CHECK_STATUS(held.table().create(held.second(), 0u, past), Status::no_space);
  CHECK(past == nullptr);
  CHECK(held.table().invariants_hold());
  held.table().forget_queue(&held.first());
  CHECK_EQ(held.table().live(), 0u);
  CHECK_STATUS(held.table().create(held.second(), 0u, past), Status::ok); }

/* A doorbell the platform cannot find memory for is no doorbell, and the table
 * is left as it was. */
TEST(doorbell_table, a_doorbell_with_no_memory_behind_it_is_refused)
{ Doorbells held;
  held.platform().fail_allocation_at(1u);
  void *made = &held;
  CHECK(held.table().create(held.first(), 1u, made) != Status::ok);
  CHECK(made == nullptr);
  CHECK_EQ(held.table().live(), 0u);
  CHECK(held.table().invariants_hold());
  CHECK_STATUS(held.table().create(held.first(), 1u, made), Status::ok); }

/* What rvk-inspect reads out of a running driver (docs/trace.md): a line for
 * the table and one per doorbell, and nothing but `busy` while another thread
 * holds it - a report must never wait on the path a submission takes. */
TEST(doorbell_table_report, writes_each_doorbell_and_says_when_it_cannot)
{ Doorbells held;
  void     *made = nullptr;
  CHECK_STATUS(held.table().create(held.first(), 2u, made), Status::ok);
  void          *address = nullptr;
  DoorbellStatus status = DoorbellStatus::disconnected_abort;
  HwQueue       *queue = nullptr;
  CHECK_STATUS(held.table().connect(made, address, status, queue), Status::ok);
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  held.table().report(writer);
  char expected[256];
  static_cast<void>(std::snprintf(expected, sizeof expected,
                                  "doorbell_table/0 busy=false live=1\n"
                                  "doorbell_table/0/doorbell/0 handle=0x%016llX queue=0x%016llX page=2 "
                                  "connected=true",
                                  static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(made)),
                                  static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&held.first()))));
  CHECK(std::strcmp(writer.dump(), expected) == 0);

  held.platform().hold_elsewhere(av::LockRank::doorbell_table);
  av::StateWriter busy(bytes, sizeof bytes);
  held.table().report(busy);
  CHECK(std::strcmp(busy.dump(), "doorbell_table/0 busy=true") == 0);
  held.platform().set_irql(av::Irql::dispatch);
  held.table().report(busy);
  CHECK(held.platform().claim(av::Violation::irql_ceiling));
  CHECK(held.platform().claim(av::Violation::irql_ceiling));   /* the try-acquire inside it */
  held.platform().set_irql(av::Irql::passive);
  held.platform().released_elsewhere();
  CHECK_STATUS(held.table().destroy(made), Status::ok); }
