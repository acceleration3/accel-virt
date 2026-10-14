#include "miniport/pci/mapped_register_window.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/gpu_command_queue.hpp"
#include "miniport/virtio/virtio_pci_transport.hpp"
#include "miniport/virtio/virtq_segment.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include <cstdint>
#include <cstring>

using av::FakeVirtqueueDevice;
using av::GpuCommandQueue;
using av::GpuRequestWriter;
using av::Irql;
using av::LockRank;
using av::MappedRegisterWindow;
using av::SegmentDirection;
using av::VirtqSegment;
using av::Status;
using av::VirtioPciTransport;
using av::Violation;

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t err_unspec = 0x1200u;
constexpr std::uint16_t flag_write = 2u;

av::Status configure(av::MappedVirtioFunction &mapped, std::uint16_t queue_size)
{
    mapped.common().set_queues(2u, queue_size);
    return mapped.map();
}

/* A started transport with a control-style queue (0, with responses) and a
 * cursor-style one (1, without). */
class Fixture {
public:
    explicit Fixture(std::uint16_t queue_size = 256u)
        : mapped_(), mapped_status_(configure(mapped_, queue_size)),
          common_(mapped_.platform(), mapped_.mappings().address_of(mapped_.layout().common()),
                  mapped_.layout().common().length()),
          notify_(mapped_.platform(), mapped_.mappings().address_of(mapped_.layout().notify()),
                  mapped_.layout().notify().length()),
          isr_(mapped_.platform(), mapped_.mappings().address_of(mapped_.layout().isr()), mapped_.layout().isr().length()),
          device_(mapped_.platform(), mapped_.mappings().address_of(mapped_.layout().device()),
                  mapped_.layout().device().length()),
          transport_(mapped_.platform(), common_, notify_, isr_, device_, mapped_.layout().notify_multiplier()),
          control_(mapped_.platform(), transport_, 0u, GpuRequestWriter::response_size,
                   GpuRequestWriter::max_backing_request_size, LockRank::control_queue),
          cursor_(mapped_.platform(), transport_, 1u, 0u, 0u, LockRank::cursor_queue)
    {
        CHECK_STATUS(mapped_status_, Status::ok);
        CHECK_STATUS(transport_.negotiate(0u, VirtioPciTransport::feature_version_1), Status::ok);
    }

    Status start()
    {
        Status status = control_.activate(0u);
        if (status == Status::ok) status = cursor_.activate(1u);
        return status == Status::ok ? transport_.start() : status;
    }

    av::MockPlatform   &platform() { return mapped_.platform(); }
    GpuCommandQueue    &control() { return control_; }
    GpuCommandQueue    &cursor() { return cursor_; }
    FakeVirtqueueDevice device(std::uint16_t queue) { return mapped_.queue_device(queue); }

private:
    av::MappedVirtioFunction mapped_;
    Status                   mapped_status_;
    MappedRegisterWindow     common_;
    MappedRegisterWindow     notify_;
    MappedRegisterWindow     isr_;
    MappedRegisterWindow     device_;
    VirtioPciTransport       transport_;
    GpuCommandQueue          control_;
    GpuCommandQueue          cursor_;
};

std::uint8_t request_bytes[GpuRequestWriter::max_request_size];
/* A backing request with more memory entries than a narrow slot could hold. */
std::uint8_t wide_request_bytes[GpuRequestWriter::max_backing_request_size];

std::size_t move_request(std::int32_t x)
{
    GpuRequestWriter writer(request_bytes, sizeof request_bytes);
    static_cast<void>(writer.move_cursor(0u, x, 0, 1u));
    return writer.length();
}

}  // namespace

TEST(gpu_command_queue_send, publishes_a_request_with_its_response_buffer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const std::size_t length = move_request(7);
  CHECK_STATUS(fixture.control().send(request_bytes, length), Status::ok);
  CHECK_EQ(fixture.control().in_flight(), 1u);
  CHECK(fixture.control().invariants_hold());
  FakeVirtqueueDevice device = fixture.device(0u);
  CHECK_EQ(device.respond_all(ok_nodata), 1u);
  CHECK_EQ(device.seen_count(), 2u);
  CHECK_EQ(device.seen(0u).length, length);
  CHECK_EQ(device.seen(1u).length, GpuRequestWriter::response_size);
  CHECK_EQ(device.seen(1u).flags & flag_write, flag_write);
  CHECK_EQ(device.request_type(0u), 0x0301u);
  CHECK_EQ(device.request(0u)[28], 7u);
  std::uint32_t completed = 0u, refused = 9u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(completed, 1u);
  CHECK_EQ(refused, 0u);
  CHECK_EQ(fixture.control().in_flight(), 0u); }

TEST(gpu_command_queue_send, a_queue_without_responses_publishes_the_request_alone)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.cursor().send(request_bytes, move_request(1)), Status::ok);
  FakeVirtqueueDevice device = fixture.device(1u);
  CHECK_EQ(device.respond_all(ok_nodata), 1u);
  CHECK_EQ(device.seen_count(), 1u);
  CHECK_EQ(device.seen(0u).flags & flag_write, 0u);
  std::uint32_t completed = 0u, refused = 9u;
  CHECK_STATUS(fixture.cursor().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, 1u);
  CHECK_EQ(refused, 0u); }

TEST(gpu_command_queue_harvest, counts_refusals_and_short_responses)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  FakeVirtqueueDevice device = fixture.device(0u);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(1)), Status::ok);
  CHECK_EQ(device.respond_all(err_unspec), 1u);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(2)), Status::ok);
  CHECK_EQ(device.complete_all(4u), 1u);
  std::uint32_t completed = 0u, refused = 0u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, 2u);
  CHECK_EQ(refused, 2u); }

TEST(gpu_command_queue_send, every_slot_in_flight_is_no_space_until_harvested)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t slot = 0u; slot < GpuCommandQueue::slot_count; slot++)
      CHECK_STATUS(fixture.control().send(request_bytes, move_request(static_cast<std::int32_t>(slot))), Status::ok);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(99)), Status::no_space);
  CHECK_EQ(fixture.control().in_flight(), GpuCommandQueue::slot_count);
  FakeVirtqueueDevice device = fixture.device(0u);
  CHECK_EQ(device.respond_all(ok_nodata), GpuCommandQueue::slot_count);
  std::uint32_t completed = 0u, refused = 0u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, GpuCommandQueue::slot_count);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(100)), Status::ok);
  CHECK_EQ(device.respond_all(ok_nodata), 1u);
  CHECK_EQ(device.request(GpuCommandQueue::slot_count)[28], 100u); }

TEST(gpu_command_queue_send, refuses_before_activation_and_out_of_bounds_requests)
{ Fixture fixture;
  std::uint32_t completed = 9u, refused = 9u;
  CHECK_STATUS(fixture.control().send(request_bytes, 8u), Status::invalid_state);
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::invalid_state);
  CHECK_EQ(completed, 0u);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.control().send(request_bytes, 0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().send(nullptr, 8u), Status::invalid_parameter);
  /* Wider than the one wide slot, and wider than any slot of a queue that has
   * none: refused, not truncated. */
  CHECK_STATUS(fixture.control().send(wide_request_bytes, sizeof wide_request_bytes + 1u), Status::too_large);
  CHECK_STATUS(fixture.cursor().send(request_bytes, GpuRequestWriter::max_request_size + 1u), Status::too_large);
  CHECK_STATUS(fixture.control().activate(0u), Status::invalid_state);
  CHECK_EQ(fixture.control().in_flight(), 0u); }

TEST(gpu_command_queue_harvest, a_lying_device_is_invalid_state)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  FakeVirtqueueDevice device = fixture.device(0u);
  device.write_used(0u, 5u, 0u);
  device.set_used_index(1u);
  std::uint32_t completed = 9u, refused = 9u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::invalid_state);
  CHECK_EQ(completed, 0u); }

TEST(gpu_command_queue_activate, out_of_memory_leaves_it_inactive)
{ Fixture fixture;
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(fixture.control().activate(0u), Status::no_memory);
  CHECK_STATUS(fixture.control().send(request_bytes, 8u), Status::invalid_state);
  fixture.platform().fail_allocation_at(2u);
  CHECK_STATUS(fixture.control().activate(0u), Status::no_memory);
  fixture.platform().fail_allocation_at(0u);
  CHECK_STATUS(fixture.control().send(request_bytes, 8u), Status::invalid_state);
  CHECK_EQ(fixture.control().in_flight(), 0u); }

TEST(gpu_command_queue, ceilings)
{ Fixture fixture;
  std::uint32_t completed = 0u, refused = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.control().activate(0u));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.control().send(request_bytes, 8u));
  static_cast<void>(fixture.control().harvest(completed, refused));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {} }

TEST(gpu_command_queue_send, a_queue_smaller_than_its_slots_runs_out_of_descriptors_first)
{ Fixture fixture(8u);
  CHECK_STATUS(fixture.start(), Status::ok);
  for (std::uint32_t request = 0u; request < 4u; request++)
      CHECK_STATUS(fixture.control().send(request_bytes, move_request(1)), Status::ok);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(1)), Status::no_space);
  CHECK_EQ(fixture.control().in_flight(), 4u);
  CHECK(fixture.control().invariants_hold()); }

/* The backing of a primary is the one request that does not fit a narrow slot:
 * it goes in the wide one, and a second cannot start until the first is done. */
TEST(gpu_command_queue_send, a_wide_request_uses_the_wide_slot_alone)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const std::size_t wide = GpuRequestWriter::max_request_size + 16u;
  std::memset(wide_request_bytes, 0xA5, sizeof wide_request_bytes);
  CHECK_STATUS(fixture.control().send(wide_request_bytes, wide), Status::ok);
  CHECK_EQ(fixture.control().in_flight(), 1u);
  CHECK_STATUS(fixture.control().send(wide_request_bytes, wide), Status::no_space);
  /* A narrow request still has its own slots. */
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(1)), Status::ok);
  FakeVirtqueueDevice device = fixture.device(0u);
  CHECK_EQ(device.respond_all(ok_nodata), 2u);
  CHECK_EQ(device.seen(0u).length, wide);
  CHECK_EQ(device.request(0u)[wide - 1u], 0xA5u);
  std::uint32_t completed = 0u, refused = 0u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, 2u);
  CHECK_EQ(refused, 0u);
  CHECK_STATUS(fixture.control().send(wide_request_bytes, wide), Status::ok);
  CHECK(fixture.control().invariants_hold()); }

namespace {

/* The device answering while the driver waits: an exchange blocks, so the only
 * point a test can answer is while it stalls or sleeps. */
class Answering {
public:
    Answering(Fixture &fixture, std::uint32_t type, std::uint32_t after_polls)
        : fixture_(fixture), device_(fixture.device(0u)), type_(type), left_(after_polls)
    {
        fixture_.platform().on_stall(&Answering::stall, this);
    }
    ~Answering() { fixture_.platform().on_stall(nullptr, nullptr); }

    std::uint16_t answered() const { return answered_; }
    FakeVirtqueueDevice &device() { return device_; }

private:
    static void stall(void *context)
    {
        auto *const self = static_cast<Answering *>(context);
        if (self->left_ != 0u) {
            self->left_--;
            return;
        }
        self->answered_ = static_cast<std::uint16_t>(self->answered_ + self->device_.respond_all(self->type_));
    }

    Fixture             &fixture_;
    FakeVirtqueueDevice  device_;
    std::uint32_t        type_;
    std::uint32_t        left_;
    std::uint16_t        answered_ = 0u;
};

}  // namespace

/* GET_DISPLAY_INFO is asked and answered before dxgkrnl has connected the
 * interrupt, so the exchange harvests the queue itself. */
TEST(gpu_command_queue_exchange, waits_for_the_answer_to_its_own_request)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Answering host(fixture, ok_nodata, 2u);
  std::uint8_t answer[GpuRequestWriter::max_response_size] = {};
  std::uint32_t written = 0u;
  CHECK_STATUS(fixture.control().exchange(request_bytes, move_request(7), answer, sizeof answer, written),
               Status::ok);
  CHECK_EQ(written, GpuRequestWriter::response_size);
  CHECK_EQ(answer[0], 0x00u);
  CHECK_EQ(answer[1], 0x11u);
  CHECK_EQ(host.answered(), 1u);
  CHECK_EQ(fixture.control().in_flight(), 0u);
  CHECK(fixture.control().invariants_hold()); }

namespace {

constexpr std::uint64_t millisecond = 10000u;   /* the interrupt clock's 100 ns ticks */

/* A host that answers once the request has waited `delay` on the guest's
 * clock - the host GPU allocating, QEMU's main loop busy - and, as the
 * interrupt would, has the DPC harvest the answer when `interrupts` says the
 * DPC runs. */
class LateHost {
public:
    LateHost(Fixture &fixture, std::uint64_t delay, bool interrupts)
        : fixture_(fixture), device_(fixture.device(0u)), delay_(delay), interrupts_(interrupts)
    {
        fixture_.platform().on_stall(&LateHost::act, this);
    }
    ~LateHost() { fixture_.platform().on_stall(nullptr, nullptr); }

    /* When the request was seen, the next one counting from now. */
    void          again() { seen_ = false; }
    std::uint64_t answered_at() const { return answered_at_; }
    std::uint32_t dpc_harvests() const { return dpc_harvests_; }

private:
    static void act(void *context)
    {
        auto *const self = static_cast<LateHost *>(context);
        const std::uint64_t now = self->fixture_.platform().interrupt_time();
        if (!self->seen_) {
            self->seen_ = true;
            self->asked_at_ = now;
        }
        if (now - self->asked_at_ < self->delay_ || self->device_.respond_all(ok_nodata) == 0u) return;
        self->answered_at_ = now;
        if (!self->interrupts_) return;
        self->fixture_.platform().set_irql(Irql::dispatch);
        std::uint32_t completed = 0u, refused = 0u;
        CHECK_STATUS(self->fixture_.control().harvest(completed, refused), Status::ok);
        self->dpc_harvests_++;
        self->fixture_.platform().set_irql(Irql::passive);
    }

    Fixture            &fixture_;
    FakeVirtqueueDevice device_;
    std::uint64_t       delay_;
    bool                interrupts_;
    bool                seen_ = false;
    std::uint64_t       asked_at_ = 0u;
    std::uint64_t       answered_at_ = 0u;
    std::uint32_t       dpc_harvests_ = 0u;
};

Status exchange_move(Fixture &fixture, std::uint32_t &written)
{
    std::uint8_t answer[GpuRequestWriter::max_response_size] = {};
    return fixture.control().exchange(request_bytes, move_request(7), answer, sizeof answer, written);
}

}  // namespace

/* An RTX 3070 Ti took 73 ms to allocate the 128 MiB a RESOURCE_CREATE_BLOB
 * asked for, and QEMU's main loop can be later still: an answer well past the
 * hundred milliseconds the driver once gave up at is an answer. The wait
 * spins only briefly and sleeps the rest, and the DPC's harvest of the answer
 * is what wakes it - not the next timeout. */
TEST(gpu_command_queue_exchange, sleeps_through_host_work_until_the_dpc_finds_the_answer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  LateHost host(fixture, 250u * millisecond, true);
  std::uint32_t written = 0u;
  CHECK_STATUS(exchange_move(fixture, written), Status::ok);
  CHECK_EQ(written, GpuRequestWriter::response_size);
  CHECK_EQ(host.dpc_harvests(), 1u);
  /* Woken at the answer, not a sleep later. */
  CHECK_EQ(fixture.platform().interrupt_time(), host.answered_at());
  CHECK(fixture.platform().stalled_ticks() <= 2u * millisecond + 200u);
  CHECK(fixture.platform().slept_ticks() >= 240u * millisecond);
  CHECK_EQ(fixture.control().in_flight(), 0u);
  CHECK(fixture.control().invariants_hold());
  /* The next exchange finds nothing left over from this one. */
  host.again();
  CHECK_STATUS(exchange_move(fixture, written), Status::ok);
  CHECK(fixture.control().invariants_hold());
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

/* Before dxgkrnl connects the interrupt nothing wakes the sleeper: it harvests
 * for itself after each sleep, so the answer is taken a sleep late at most. */
TEST(gpu_command_queue_exchange, without_the_interrupt_harvests_for_itself_between_sleeps)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  LateHost host(fixture, 30u * millisecond, false);
  std::uint32_t written = 0u;
  CHECK_STATUS(exchange_move(fixture, written), Status::ok);
  CHECK_EQ(written, GpuRequestWriter::response_size);
  CHECK(fixture.platform().interrupt_time() >= host.answered_at());
  CHECK(fixture.platform().interrupt_time() - host.answered_at() <= 10u * millisecond);
  CHECK(fixture.platform().stalled_ticks() <= 2u * millisecond + 200u);
  CHECK(fixture.control().invariants_hold()); }

/* An answer within the spin is taken without sleeping: what nearly every
 * control request is, and what StartDevice's exchanges are before the
 * interrupt is connected. */
TEST(gpu_command_queue_exchange, a_quick_answer_is_taken_without_sleeping)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  LateHost host(fixture, millisecond, false);
  std::uint32_t written = 0u;
  CHECK_STATUS(exchange_move(fixture, written), Status::ok);
  CHECK_EQ(fixture.platform().slept_ticks(), 0u);
  CHECK(fixture.platform().interrupt_time() < 2u * millisecond); }

/* Three seconds unanswered is a device that stopped: the exchange says so, and
 * logs it once. Until that request is answered every later exchange is
 * refused at once without being sent - the device answers in order, so it
 * could not answer one - and once the device does answer it, the next
 * exchange is asked as any other. */
TEST(gpu_command_queue_exchange, answers_not_found_when_the_device_never_does)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  std::uint32_t written = 7u;
  CHECK_STATUS(exchange_move(fixture, written), Status::not_found);
  CHECK_EQ(written, 0u);
  const std::uint64_t gave_up = fixture.platform().interrupt_time();
  CHECK(gave_up >= 3000u * millisecond);
  CHECK(gave_up <= 3000u * millisecond + 10u * millisecond);
  CHECK(fixture.platform().stalled_ticks() <= 2u * millisecond + 200u);
  CHECK(fixture.platform().logged_line("left a control request unanswered"));
  CHECK_EQ(fixture.platform().logged(), 1u);
  CHECK(fixture.control().invariants_hold());
  CHECK_EQ(fixture.control().in_flight(), 1u);

  const std::uint16_t published = fixture.device(0u).available_index();
  CHECK_STATUS(exchange_move(fixture, written), Status::not_found);
  CHECK_EQ(fixture.device(0u).available_index(), published);
  CHECK_EQ(fixture.platform().interrupt_time(), gave_up);
  CHECK_EQ(fixture.platform().logged(), 1u);

  CHECK_EQ(fixture.device(0u).respond_all(ok_nodata), 1u);
  Answering host(fixture, ok_nodata, 0u);
  CHECK_STATUS(exchange_move(fixture, written), Status::ok);
  CHECK_EQ(written, GpuRequestWriter::response_size);
  CHECK_EQ(fixture.control().in_flight(), 0u);
  CHECK(fixture.control().invariants_hold()); }

/* A device that publishes a completion for nothing before the request is
 * sent: the exchange answers the harvest's failure and sends nothing. */
TEST(gpu_command_queue_exchange, a_harvest_that_fails_before_sending_sends_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  fixture.device(0u).set_used_index(3u);
  std::uint32_t written = 0u;
  CHECK_STATUS(exchange_move(fixture, written), Status::invalid_state);
  CHECK_EQ(fixture.device(0u).available_index(), 0u); }

TEST(gpu_command_queue_exchange, refuses_a_second_exchange_while_one_is_waiting)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  struct Reentry {
      Fixture      *fixture;
      Status        status = Status::ok;
      std::uint32_t written = 0u;
      std::uint8_t  answer[GpuRequestWriter::response_size] = {};
  } reentry{&fixture};
  fixture.platform().on_stall([](void *context) {
      auto *const state = static_cast<Reentry *>(context);
      state->status = state->fixture->control().exchange(request_bytes, 32u, state->answer,
                                                         sizeof state->answer, state->written);
  }, &reentry);
  std::uint8_t answer[GpuRequestWriter::response_size] = {};
  std::uint32_t written = 0u;
  CHECK_STATUS(fixture.control().exchange(request_bytes, move_request(7), answer, sizeof answer, written),
               Status::not_found);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_STATUS(reentry.status, Status::conflict); }

TEST(gpu_command_queue_exchange, refuses_what_it_cannot_send_or_answer)
{ Fixture fixture;
  std::uint8_t answer[GpuRequestWriter::response_size] = {};
  std::uint32_t written = 0u;
  CHECK_STATUS(fixture.control().exchange(request_bytes, 32u, answer, sizeof answer, written), Status::invalid_state);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.control().exchange(nullptr, 32u, answer, sizeof answer, written), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().exchange(request_bytes, 0u, answer, sizeof answer, written), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().exchange(request_bytes, 32u, nullptr, sizeof answer, written),
               Status::invalid_parameter);
  CHECK_STATUS(fixture.control().exchange(request_bytes, 32u, answer, 0u, written), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().exchange(wide_request_bytes, sizeof wide_request_bytes + 1u, answer, sizeof answer,
                                          written), Status::too_large);
  /* A queue with no answers to give cannot be asked. */
  CHECK_STATUS(fixture.cursor().exchange(request_bytes, 32u, answer, sizeof answer, written),
               Status::invalid_parameter); }

namespace av {

/* The states the invariant guards, which correct code never reaches. */
struct GpuCommandQueueProbe {
    static void lose_a_completion(GpuCommandQueue &queue) { queue.in_flight_++; }
    static void wait_on_a_free_slot(GpuCommandQueue &queue) { queue.awaited_slot_ = 0u; }
    static void abandon_a_free_slot(GpuCommandQueue &queue) { queue.abandoned_slot_ = 0u; }
    static void abandon_the_awaited_slot(GpuCommandQueue &queue) { queue.abandoned_slot_ = queue.awaited_slot_; }
    static void abandon_slot_none(GpuCommandQueue &queue) { queue.abandoned_slot_ = GpuCommandQueue::no_slot; }
};

}  // namespace av

TEST(gpu_command_queue_invariants, a_lost_completion_and_a_free_awaited_slot_are_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK(fixture.control().invariants_hold());
  av::GpuCommandQueueProbe::wait_on_a_free_slot(fixture.control());
  CHECK(!fixture.control().invariants_hold());
  Fixture other;
  CHECK_STATUS(other.start(), Status::ok);
  av::GpuCommandQueueProbe::lose_a_completion(other.control());
  CHECK(!other.control().invariants_hold());
  Fixture abandoned;
  CHECK_STATUS(abandoned.start(), Status::ok);
  av::GpuCommandQueueProbe::abandon_a_free_slot(abandoned.control());
  CHECK(!abandoned.control().invariants_hold()); }

/* The request given up on is never the one being waited for. */
TEST(gpu_command_queue_invariants, an_abandoned_slot_that_is_awaited_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  struct Probe {
      Fixture *fixture;
      bool     held = false;
  } probe{&fixture};
  fixture.platform().on_stall([](void *context) {
      auto *const state = static_cast<Probe *>(context);
      if (state->held) return;
      av::GpuCommandQueueProbe::abandon_the_awaited_slot(state->fixture->control());
      state->held = !state->fixture->control().invariants_hold();
      av::GpuCommandQueueProbe::abandon_slot_none(state->fixture->control());
      state->fixture->device(0u).respond_all(ok_nodata);
  }, &probe);
  std::uint32_t written = 0u;
  CHECK_STATUS(exchange_move(fixture, written), Status::ok);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK(probe.held); }

/* A wide request is asked and answered like any other. */
TEST(gpu_command_queue_exchange, carries_a_wide_request)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Answering host(fixture, ok_nodata, 0u);
  std::uint8_t answer[GpuRequestWriter::max_response_size] = {};
  std::uint32_t written = 0u;
  CHECK_STATUS(fixture.control().exchange(wide_request_bytes, GpuRequestWriter::max_request_size + 16u, answer,
                                          sizeof answer, written), Status::ok);
  CHECK_EQ(written, GpuRequestWriter::response_size); }

/* No room to publish the request: the exchange says so instead of waiting. */
TEST(gpu_command_queue_exchange, answers_a_queue_with_no_room)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(1)), Status::ok);
  std::uint8_t answer[GpuRequestWriter::response_size] = {};
  std::uint32_t written = 0u;
  CHECK(fixture.control().exchange(request_bytes, move_request(2), answer, sizeof answer, written) != Status::ok); }

/* A device that publishes a completion for nothing fails the harvest, and the
 * exchange answers that rather than waiting out its polls. */
TEST(gpu_command_queue_exchange, answers_a_harvest_that_failed)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  struct Lying {
      Fixture *fixture;
      bool     lied = false;
  } lying{&fixture};
  fixture.platform().on_stall([](void *context) {
      auto *const state = static_cast<Lying *>(context);
      if (state->lied) return;
      state->lied = true;
      state->fixture->device(0u).set_used_index(9u);
  }, &lying);
  std::uint8_t answer[GpuRequestWriter::response_size] = {};
  std::uint32_t written = 0u;
  CHECK(fixture.control().exchange(request_bytes, move_request(3), answer, sizeof answer, written) != Status::ok);
  fixture.platform().on_stall(nullptr, nullptr); }

/* Anything outside virtio-gpu's OK range is a refusal, whatever it is. */
TEST(gpu_command_queue_harvest, counts_an_answer_that_is_not_an_ok_as_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CHECK_STATUS(fixture.control().send(request_bytes, move_request(1)), Status::ok);
  FakeVirtqueueDevice device = fixture.device(0u);
  CHECK_EQ(device.respond_all(0x1000u), 1u);   /* a command type, not an answer */
  std::uint32_t completed = 0u, refused = 0u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, 1u);
  CHECK_EQ(refused, 1u); }

/* A request too large for any slot continues in memory of the caller's: the
 * device reads the slot's bytes, then the payload, then writes its answer -
 * one chain, and the answer is the exchange's like any other. */
TEST(gpu_command_queue_exchange, carries_a_payload_the_device_reads_after_the_request)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  constexpr std::size_t payload_bytes = 3u * 4096u;
  auto *const           payload = static_cast<std::uint8_t *>(fixture.platform().allocate_contiguous(payload_bytes));
  CHECK(payload != nullptr);
  for (std::size_t at = 0u; at < payload_bytes; at++) payload[at] = static_cast<std::uint8_t>(at * 7u);
  const std::uint64_t address = fixture.platform().physical_address(payload);
  Answering           host(fixture, ok_nodata, 1u);
  std::uint8_t        answer[GpuRequestWriter::max_response_size] = {};
  std::uint32_t       written = 0u;
  const std::size_t   length = move_request(9);
  CHECK_STATUS(fixture.control().exchange_carrying(request_bytes, length,
                                                   VirtqSegment(address, payload_bytes, SegmentDirection::device_reads),
                                                   answer, sizeof answer, written),
               Status::ok);
  CHECK_EQ(written, GpuRequestWriter::response_size);
  CHECK_EQ(answer[1], 0x11u);
  CHECK_EQ(host.answered(), 1u);
  FakeVirtqueueDevice &device = host.device();
  CHECK_EQ(device.seen_count(), 3u);
  CHECK_EQ(device.seen(0u).length, length);
  CHECK_EQ(device.seen(1u).address, address);
  CHECK_EQ(device.seen(1u).length, payload_bytes);
  CHECK_EQ(device.seen(1u).flags & flag_write, 0u);
  CHECK_EQ(device.seen(2u).length, GpuRequestWriter::response_size);
  CHECK_EQ(device.seen(2u).flags & flag_write, flag_write);
  CHECK_EQ(fixture.control().in_flight(), 0u);
  CHECK(fixture.control().invariants_hold());
  fixture.platform().release_contiguous(payload, payload_bytes); }

/* A request sent with a payload is published as the same chain an exchange
 * makes and left: the call returns before any answer, and the harvest that
 * finds it used frees its slot. */
TEST(gpu_command_queue_send, carries_a_payload_without_waiting_for_the_answer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const VirtqSegment payload(0x20000u, 512u, SegmentDirection::device_reads);
  CHECK(payload.continues_request());
  const std::size_t length = move_request(4);
  CHECK_STATUS(fixture.control().send_carrying(request_bytes, length, payload), Status::ok);
  CHECK_EQ(fixture.control().in_flight(), 1u);
  FakeVirtqueueDevice device = fixture.device(0u);
  CHECK_EQ(device.respond_all(ok_nodata), 1u);
  CHECK_EQ(device.seen_count(), 3u);
  CHECK_EQ(device.seen(0u).length, length);
  CHECK_EQ(device.seen(1u).address, 0x20000u);
  CHECK_EQ(device.seen(1u).length, 512u);
  CHECK_EQ(device.seen(1u).flags & flag_write, 0u);
  CHECK_EQ(device.seen(2u).flags & flag_write, flag_write);
  std::uint32_t completed = 0u, refused = 0u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, 1u);
  CHECK_EQ(refused, 0u);
  CHECK_EQ(fixture.control().in_flight(), 0u);
  CHECK(fixture.control().invariants_hold()); }

TEST(gpu_command_queue_send, refuses_a_payload_that_continues_nothing_or_a_request_too_wide)
{ Fixture fixture;
  const VirtqSegment payload(0x20000u, 512u, SegmentDirection::device_reads);
  CHECK_STATUS(fixture.control().send_carrying(request_bytes, move_request(1), payload), Status::invalid_state);
  CHECK_STATUS(fixture.start(), Status::ok);
  const VirtqSegment empty(0x20000u, 0u, SegmentDirection::device_reads);
  const VirtqSegment written(0x20000u, 64u, SegmentDirection::device_writes);
  CHECK(!empty.continues_request());
  CHECK(!written.continues_request());
  CHECK_STATUS(fixture.control().send_carrying(request_bytes, move_request(1), empty), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().send_carrying(request_bytes, move_request(1), written), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().send_carrying(nullptr, 8u, payload), Status::invalid_parameter);
  CHECK_STATUS(fixture.control().send_carrying(request_bytes, 0u, payload), Status::invalid_parameter);
  std::uint8_t wide[GpuRequestWriter::max_request_size + 1u] = {};
  CHECK_STATUS(fixture.control().send_carrying(wide, sizeof wide, payload), Status::too_large);
  for (std::uint32_t slot = 0u; slot < GpuCommandQueue::slot_count; slot++)
      CHECK_STATUS(fixture.control().send_carrying(request_bytes, move_request(1), payload), Status::ok);
  CHECK_STATUS(fixture.control().send_carrying(request_bytes, move_request(1), payload), Status::no_space);
  CHECK_EQ(fixture.control().in_flight(), GpuCommandQueue::slot_count); }

/* A payload of nothing, or one the device would write, is no request's
 * continuation, and nothing is published. */
TEST(gpu_command_queue_exchange, refuses_a_payload_that_is_empty_or_written_by_the_device)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  std::uint8_t  answer[GpuRequestWriter::max_response_size] = {};
  std::uint32_t written = 7u;
  CHECK_STATUS(fixture.control().exchange_carrying(request_bytes, move_request(1),
                                                   VirtqSegment(0x10000u, 0u, SegmentDirection::device_reads), answer,
                                                   sizeof answer, written),
               Status::invalid_parameter);
  CHECK_EQ(written, 0u);
  CHECK_STATUS(fixture.control().exchange_carrying(request_bytes, move_request(1),
                                                   VirtqSegment(0x10000u, 64u, SegmentDirection::device_writes), answer,
                                                   sizeof answer, written),
               Status::invalid_parameter);
  /* A queue that answers nothing cannot be asked, payload or not. */
  CHECK_STATUS(fixture.cursor().exchange_carrying(request_bytes, move_request(1),
                                                  VirtqSegment(0x10000u, 64u, SegmentDirection::device_reads), answer,
                                                  sizeof answer, written),
               Status::invalid_parameter);
  CHECK_EQ(fixture.control().in_flight(), 0u); }
