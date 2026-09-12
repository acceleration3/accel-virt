#include "core/pci/mapped_register_window.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/support/status.hpp"
#include "core/virtio/gpu_command_queue.hpp"
#include "core/virtio/virtio_pci_transport.hpp"
#include "harness/device/mapped_virtio_function.hpp"
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
    static_cast<void>(writer.move_cursor(0u, x, 0));
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
  CHECK_EQ(device.request(0u)[FakeVirtqueueDevice::request_bytes - 1u], 0xA5u);
  std::uint32_t completed = 0u, refused = 0u;
  CHECK_STATUS(fixture.control().harvest(completed, refused), Status::ok);
  CHECK_EQ(completed, 2u);
  CHECK_EQ(refused, 0u);
  CHECK_STATUS(fixture.control().send(wide_request_bytes, wide), Status::ok);
  CHECK(fixture.control().invariants_hold()); }
